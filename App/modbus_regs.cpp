#include "modbus_regs.h"

#include "thermo.h"
#include "sr_io.h"
#include "pumps.h"
#include "motor.h"
#include "nvs.h"
#include "task_handles.h"
#include "wdt.h"
#include "a_io.h"
#include "interop.h"

#include "modbus/MODBUS-LIB/Inc/Modbus.h"
#include "usbd_cdc_if.h"

#ifndef ARRAY_SIZE
    #define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#endif
#define DEFINE_REGS(name) reg_t buffer_##name ; \
    uint16_t * const p_##name = reinterpret_cast<uint16_t*>( &buffer_##name )
#define GET_BUF(name) buffer_##name
#define COPY_OUTPUT_STRUCTS(origin, dest) for (size_t ijk = 0; ijk < ARRAY_SIZE(dest); ijk++) dest[ijk] = origin[ijk]
#define COPY_INPUT_STRUCTS(origin, dest) for (size_t ijk = 0; ijk < ARRAY_SIZE(origin); ijk++) dest[ijk] = origin[ijk]
#define CHECK_ACTIVITY_BIT(bit) ((mb->p->interface_active & (1u << (bit))) > 0)
#define RESET_BIT(src, idx) src &= ~(1u << (idx))

namespace mb_regs
{
    enum interface_activity_bits : uint16_t
    {
        receive = 0,
        reload,
        save_nvs,
        reset,
        keep_alive
    };

    struct PACKED_FOR_MODBUS reg_t
    {
        uint16_t pumps_num = MY_PUMPS_NUM;
        uint16_t thermocouples_num = MY_TEMP_CHANNEL_NUM;
        uint16_t input_words_num = sr_io::input_buffer_len;
        uint16_t output_words_num = sr_io::output_buffer_len;
        uint16_t analog_input_num = a_io::in::LEN;

        uint16_t status = 0;
        uint16_t interface_active = 0;
        uint16_t addr = 1; //Len = 1
        float max6675_temps[MY_TEMP_CHANNEL_NUM];
        float analog_inputs[a_io::in::LEN] = {};
        a_io::cal_t analog_cal[a_io::in::LEN] = {};
        sr_buf_t sr_inputs[sr_io::input_buffer_len]; //Len = 1
        sr_buf_t sr_outputs[sr_io::output_buffer_len]; //Len = 2
        sr_buf_t commanded_outputs[sr_io::output_buffer_len]; //Len = 2
        uint16_t reserved1;
        pumps::params_t pump_params;
        motor_params_t motor_params[MY_PUMPS_NUM];
        motor_reg_t motor_regs[MY_PUMPS_NUM];
        float commanded_motor_rate[MY_PUMPS_NUM];
    };
    const size_t length = sizeof(reg_t) / sizeof(uint16_t);

    DEFINE_REGS(MY_MB_USB);

    struct instance_t
    {
        reg_t* p;
        modbusHandler_t cfg;
        TickType_t last_keepalive_check;
        uint16_t keepalive_counter;
    };
    static instance_t instances[] = {
        { 
            .p = &GET_BUF(MY_MB_USB), 
            .cfg = {
                .uModbusType = MB_SLAVE,
                .port = NULL,
                //.u8id = 1,
                .EN_Port = NULL,
                .u16regs = GET_MB_REGS_PTR(MY_MB_USB),
                .u16timeOut = 1000,
                .u16regsize = length,
                .xTypeHW = mb_hardware_t::USB_CDC_HW
            },
            .last_keepalive_check = configINITIAL_TICK_COUNT,
            .keepalive_counter = 0
        }
    };
    static uint16_t* mb_addr;
    static bool remote_enable = false;
    static SemaphoreHandle_t set_mutex = NULL;
    static uint16_t status_double_buffer = 0;

    void receive_callback(uint8_t* data, uint32_t* length)
    {
        printf(">RX+%lu\n", *length);
    }
    void transmit_callback(uint8_t* data, uint32_t length)
    {
        printf(">TX+%lu\n", length);
    }
    void print_dbg(bool install_callbacks)
    {
        static bool callback_registered = false;

        if (callback_registered != install_callbacks)
        {
            CDC_Register_RX_Callback(install_callbacks ? receive_callback : modbus_cdc_rx_callback);
            CDC_Register_TX_Callback(install_callbacks ? transmit_callback : NULL);
            callback_registered = install_callbacks;
        }
        printf("\tCDC connected = %hu\n"
            "\tCDC can transmit = %hu\n"
            "\tRX callback installed = %u\n",
            CDC_IsConnected(),
            CDC_Can_Transmit(),
            callback_registered
        );
        for (size_t i = 0; i < array_size(instances); i++)
        {
            auto& c = instances[i].cfg;
            printf("\t#%u:\n"
                "\t\tErr count = %u\n"
                "\t\tLast err = %hi\n"
                "\t\tIn count = %u\n"
                "\t\tOut count = %u\n"
                "\t\tState = %hi\n"
                "\t\tBuffer len = %hu\n"
                "\t\tAddress mismatch cnt = %u\n"
                "\t\tInterface act = %u\n",
                i,
                c.u16errCnt,
                c.i8lastError,
                c.u16InCnt,
                c.u16OutCnt,
                c.i8state,
                c.u8BufferSize,
                c.addrMissmatchCnt,
                instances[i].p->interface_active
            );
        }
    }

    void sync_instance(instance_t* mb)
    {
        /** OUTPUT **/
        //Status
        mb->p->status = status_double_buffer;

        //MAX6675 sensors
        auto temps = thermo::get_temperatures();
        COPY_OUTPUT_STRUCTS(temps, mb->p->max6675_temps);

        //GPIO
        auto gpio = sr_io::get_inputs();
        COPY_OUTPUT_STRUCTS(gpio, mb->p->sr_inputs);
        gpio = sr_io::get_outputs();
        COPY_OUTPUT_STRUCTS(gpio, mb->p->sr_outputs);

        //Pump values
        auto motor_regs = nvs::get_motor_regs();
        for (size_t i = 0; i < MY_PUMPS_NUM; i++)
        {
            if (pumps::get_running(i))
                motor_regs[i].status |= (1u << motor_t::status_bits::running);
            else
                motor_regs[i].status &= ~(1u << motor_t::status_bits::running);
        }
        COPY_OUTPUT_STRUCTS(motor_regs, mb->p->motor_regs);

        /** INPUT **/
        if (remote_enable && ((mb->p->interface_active & (1u << interface_activity_bits::receive)) > 0))
        {
            static const TickType_t keepalive_delay = pdMS_TO_TICKS(1000);

            if (CHECK_ACTIVITY_BIT(interface_activity_bits::reset))
            {
                vTaskDelay(pdMS_TO_TICKS(100));
                HAL_NVIC_SystemReset();
            }
            TickType_t now = xTaskGetTickCount();
            if ((now - mb->last_keepalive_check) > keepalive_delay)
            {
                mb->last_keepalive_check = now;
                if (CHECK_ACTIVITY_BIT(interface_activity_bits::keep_alive)) mb->keepalive_counter = 0;
                else mb->keepalive_counter++;
                RESET_BIT(mb->p->interface_active, interface_activity_bits::keep_alive);
                if (mb->keepalive_counter > *nvs::get_modbus_keepalive_threshold())
                {
                    RESET_BIT(mb->p->interface_active, interface_activity_bits::receive);
                    interop::enqueue(interop::cmds::modbus_keepalive_failed, NULL);
                }
            }

            //Parameters are only modified if reload bit is set
            //Otherwise only IO and pump setpoints are tracked
            if (CHECK_ACTIVITY_BIT(interface_activity_bits::reload))
            {
                //Modbus address
                *mb_addr = mb->p->addr;
                //Pump config
                auto motor_params = nvs::get_motor_params();
                COPY_INPUT_STRUCTS(mb->p->motor_params, motor_params);
                *nvs::get_pump_params() = mb->p->pump_params;

                pumps::reload_params();
                pumps::reload_motor_params();
                RESET_BIT(mb->p->interface_active, interface_activity_bits::reload);
            }

            //Commanded GPIO
            //TODO

            //Pump commanded values
            for (size_t i = 0; i < MY_PUMPS_NUM; i++)
            {
                pumps::set_indicated_speed(i, mb->p->commanded_motor_rate[i]);
            }

            //NVS save bit
            if (CHECK_ACTIVITY_BIT(interface_activity_bits::save_nvs))
            {
                nvs::save();
                RESET_BIT(mb->p->interface_active, interface_activity_bits::save_nvs);
            }
        }
        else
        {
            //Update all params so that hosts can at least receive them if remote is not enabled
            mb->p->addr = *mb_addr;
            mb->p->pump_params = *nvs::get_pump_params();
            auto motor_params = nvs::get_motor_params();
            COPY_OUTPUT_STRUCTS(motor_params, mb->p->motor_params);
            RESET_BIT(mb->p->interface_active, interface_activity_bits::keep_alive);
            mb->keepalive_counter = 0;
            mb->last_keepalive_check = xTaskGetTickCount();
        }
    }

    HAL_StatusTypeDef init(uint16_t* addr)
    {
        static_assert(sizeof(reg_t) % 2 == 0); //32-bit alignment
        DBG("Modbus init...");
        assert_param(!set_mutex);

        set_mutex = xSemaphoreCreateMutex();
        assert_param(set_mutex);
        mb_addr = addr;
        CDC_Register_RX_Callback(modbus_cdc_rx_callback);
        for (auto &&i : instances)
        {
            i.p->addr = *addr;
            i.cfg.u8id = static_cast<uint8_t>(*addr);
            ModbusInit(&(i.cfg));
        }
        ModbusStartCDC(&(instances[MY_MB_USB].cfg));
        for (size_t i = 0; i < array_size(instances); i++)
        {
            if (i == MY_MB_USB) continue;
            ModbusStart(&(instances[i].cfg));
        }
        return set_mutex ? HAL_OK : HAL_ERROR;
    }

    HAL_StatusTypeDef sync()
    {
        /**
         * Output registers are overwritten on every sync
         * Input registers only matter if interface activity bit is set for corresponding modbus instance
         * The sync is protected with a mutex, as well as internal modbus instance operations
        */

        size_t i;
        for (i = 0; i < array_size(instances); i++)
        {
            if (osSemaphoreAcquire(instances[i].cfg.ModBusSphrHandle, 10) != osOK) break;
        }
        if (i < array_size(instances))
        {
            for (--i; i >= 0; i--)
            {
                osSemaphoreRelease(instances[i].cfg.ModBusSphrHandle);
            }
            return HAL_BUSY;
        }
        while (xSemaphoreTake(set_mutex, portMAX_DELAY) != pdTRUE);

        i = 0;
        for (auto &&j : instances)
        {
            instance_t* mb = &j;
            if (CHECK_ACTIVITY_BIT(interface_activity_bits::receive)) i++;
        }
        if (i > 1)
        {
            for (auto &&j : instances)
            {
                RESET_BIT(j.p->interface_active, interface_activity_bits::receive);
            }
        }
        /*else if ((i == 0) && remote_enable) // All down
        {
            pumps::stop_all();
        }*/
        for (auto &&j : instances)
        {
            sync_instance(&j);
        }

        xSemaphoreGive(set_mutex);
        for (i = 0; i < array_size(instances); i++)
        {
            osSemaphoreRelease(instances[i].cfg.ModBusSphrHandle);
        }
        return HAL_OK;
    }

    HAL_StatusTypeDef set_remote(bool enable)
    {
        if (enable == remote_enable) return HAL_OK;
        if (xSemaphoreTake(set_mutex, pdMS_TO_TICKS(10)) != pdTRUE) return HAL_BUSY;
        remote_enable = enable;
        xSemaphoreGive(set_mutex);
        return HAL_OK;
    }
    HAL_StatusTypeDef set_status(uint16_t s)
    {
        if (s == status_double_buffer) return HAL_OK;
        if (xSemaphoreTake(set_mutex, pdMS_TO_TICKS(10)) != pdTRUE) return HAL_BUSY;
        status_double_buffer = s;
        xSemaphoreGive(set_mutex);
        return HAL_OK;
    }
    bool any_remote_ready()
    {
        for (auto &&i : instances)
        {
            instance_t* mb = &i;
            if (CHECK_ACTIVITY_BIT(interface_activity_bits::receive)) return true;
        }
        return false;
    }
} // namespace mb_regs

_BEGIN_STD_C
STATIC_TASK_BODY(MY_MODBUS)
{
    const uint32_t normal_delay = 400;
    const uint32_t missed_delay = 100;
    static uint32_t delay = normal_delay;
    static TickType_t last_wake;
    static wdt::task_t* pwdt;

    if (mb_regs::init(nvs::get_modbus_addr()) != HAL_OK) while(1);
    pwdt = wdt::register_task(1000, "mb");
    INIT_NOTIFY(MY_MODBUS);

    last_wake = xTaskGetTickCount();
    for (;;)
    {
        delay = mb_regs::sync() == HAL_BUSY ? missed_delay : normal_delay;

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(delay));
        pwdt->last_time = last_wake;
    }
}
_END_STD_C