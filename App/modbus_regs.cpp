#include "modbus_regs.h"

#include "thermo.h"
#include "sr_io.h"
#include "pumps.h"
#include "motor.h"
#include "nvs.h"
#include "task_handles.h"
#include "wdt.h"

#include "modbus/MODBUS-LIB/Inc/Modbus.h"

#ifndef ARRAY_SIZE
    #define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#endif
#define DEFINE_REGS(name) reg_t buffer_##name ; \
    uint16_t * const p_##name = reinterpret_cast<uint16_t*>( &buffer_##name )
#define GET_BUF(name) buffer_##name
#define COPY_OUTPUT_STRUCTS(origin, dest) for (size_t ijk = 0; ijk < ARRAY_SIZE(dest); ijk++) dest[ijk] = origin[ijk]
#define COPY_INPUT_STRUCTS(origin, dest) for (size_t ijk = 0; ijk < ARRAY_SIZE(origin); ijk++) dest[ijk] = origin[ijk]
#define CHECK_ACTIVITY_BIT(bit) ((p->interface_active & (1u << (bit))) > 0)
#define RESET_BIT(src, idx) src &= ~(1u << (idx))

namespace mb_regs
{
    enum interface_activity_bits : uint16_t
    {
        receive = 0,
        reload,
        save_nvs,
        reset
    };

    struct PACKED_FOR_MODBUS reg_t
    {
        uint16_t status = 0;
        uint16_t interface_active = 0;
        float max6675_temps[MY_TEMP_CHANNEL_NUM];
        uint16_t addr = 1; //Len = 1
        sr_buf_t sr_inputs[sr_io::input_buffer_len]; //Len = 1
        sr_buf_t sr_outputs[sr_io::output_buffer_len]; //Len = 2
        sr_buf_t commanded_outputs[sr_io::output_buffer_len]; //Len = 2
        pumps::params_t pump_params;
        motor_params_t motor_params[MY_PUMPS_MAX];
        motor_reg_t motor_regs[MY_PUMPS_MAX];
        float commanded_motor_rate[MY_PUMPS_MAX];
    };
    const size_t length = sizeof(reg_t) / sizeof(uint16_t);

    DEFINE_REGS(MY_MB_USB);

    struct instance_t
    {
        reg_t* p;
        modbusHandler_t cfg;
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
                .u16timeOut = 300,
                .u16regsize = length,
                .xTypeHW = mb_hardware_t::USB_CDC_HW
            }
        }
    };
    static uint16_t* mb_addr;
    static bool remote_enable = false;
    static SemaphoreHandle_t set_mutex = NULL;
    static uint16_t status_double_buffer = 0;

    void sync_instance(reg_t* p, osSemaphoreId_t mb_sem)
    {
        /** OUTPUT **/
        //Status
        p->status = status_double_buffer;

        //MAX6675 sensors
        auto temps = thermo::get_temperatures();
        COPY_OUTPUT_STRUCTS(temps, p->max6675_temps);

        //GPIO
        auto gpio = sr_io::get_inputs();
        COPY_OUTPUT_STRUCTS(gpio, p->sr_inputs);
        gpio = sr_io::get_outputs();
        COPY_OUTPUT_STRUCTS(gpio, p->sr_outputs);

        //Pump values
        auto motor_regs = nvs::get_motor_regs();
        COPY_OUTPUT_STRUCTS(motor_regs, p->motor_regs);

        /** INPUT **/
        if (!remote_enable) return;
        if ((p->interface_active & (1u << interface_activity_bits::receive)) == 0) return;

        if (CHECK_ACTIVITY_BIT(interface_activity_bits::reset))
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            HAL_NVIC_SystemReset();
        }

        //Parameters are only modified if reload bit is set
        //Otherwise only IO and pump setpoints are tracked
        if (CHECK_ACTIVITY_BIT(interface_activity_bits::reload))
        {
            //Modbus address
            *mb_addr = p->addr;
            //Pump config
            auto motor_params = nvs::get_motor_params();
            COPY_INPUT_STRUCTS(p->motor_params, motor_params);
            *nvs::get_pump_params() = p->pump_params;

            pumps::reload_params();
            pumps::reload_motor_params();
            RESET_BIT(p->interface_active, interface_activity_bits::reload);
        }

        //Commanded GPIO
        //TODO

        //Pump commanded values
        for (size_t i = 0; i < MY_PUMPS_NUM; i++)
        {
            pumps::set_indicated_speed(i, p->commanded_motor_rate[i]);
        }

        //NVS save bit
        if (CHECK_ACTIVITY_BIT(interface_activity_bits::save_nvs))
        {
            nvs::save();
            RESET_BIT(p->interface_active, interface_activity_bits::save_nvs);
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
            auto p = j.p;
            if (CHECK_ACTIVITY_BIT(interface_activity_bits::receive)) i++;
        }
        if (i > 1)
        {
            for (auto &&j : instances)
            {
                RESET_BIT(j.p->interface_active, interface_activity_bits::receive);
            }
        }
        for (auto &&j : instances)
        {
            sync_instance(j.p, j.cfg.ModBusSphrHandle);
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
} // namespace mb_regs

_BEGIN_STD_C
STATIC_TASK_BODY(MY_MODBUS)
{
    const uint32_t normal_delay = 400;
    const uint32_t missed_delay = 100;
    static uint32_t delay = normal_delay;
    static TickType_t last_wake;
    static wdt::task_t* pwdt;

    pwdt = wdt::register_task(1000);
    if (mb_regs::init(nvs::get_modbus_addr()) != HAL_OK) while(1);

    for (;;)
    {
        delay = mb_regs::sync() == HAL_BUSY ? missed_delay : normal_delay;

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(delay));
        pwdt->last_time = last_wake;
    }
}
_END_STD_C