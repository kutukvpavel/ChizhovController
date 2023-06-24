#include "coprocessor.h"

#include "task_handles.h"
#include "wdt.h"
#include "i2c_sync.h"
#include "pumps.h"
#include "crc.h"
#include "average/average.h"

#include <math.h>

#define MAX_ENCODERS 3
#define AVERAGING 20
#define COPROCESSOR_ADDR (0x08 << 1u)
#define COPROCESSOR_INIT_BYTE 0xA0
#define PACKED_FOR_I2C __packed __aligned(sizeof(uint32_t))

#define ACQUIRE_MUTEX() while (xSemaphoreTake(sync_mutex, portMAX_DELAY) != pdTRUE)
#define RELEASE_MUTEX() xSemaphoreGive(sync_mutex)

namespace coprocessor
{
    struct PACKED_FOR_I2C memory_map_t
    {
        uint16_t encoder_pos[MAX_ENCODERS];
        uint16_t manual_override;
        uint8_t encoder_btn_pressed;
        uint8_t drv_error_bitfield;
        uint8_t drv_missing_bitfield;
        uint8_t drv_load[MY_PUMPS_NUM];
        uint8_t reserved1;
        uint8_t crc;
    };
    static volatile memory_map_t buffer1 = {};
    static volatile memory_map_t buffer2 = {};
    static volatile memory_map_t* filled_buffer = &buffer1;
    static volatile memory_map_t* standby_buffer = &buffer2;
    static StaticSemaphore_t sync_mutex_buffer;
    static SemaphoreHandle_t sync_mutex = NULL;
    static bool initialized = false;
    static uint32_t crc_err_stats = 0;
    static average load_average[MY_PUMPS_NUM] = { average(AVERAGING), average(AVERAGING), average(AVERAGING) };

    HAL_StatusTypeDef init()
    {
        DBG("Coprocessor init...");
        sync_mutex = xSemaphoreCreateMutexStatic(&sync_mutex_buffer);
        if (!sync_mutex) return HAL_ERROR;

        return reinit();
    }
    HAL_StatusTypeDef reinit()
    {
        // Actual write only affects coprocessor status LED
        //  This piece is mainly needed to prevent manual mode from fetching initial zero-initialized variables
        //  Before the very first read
        HAL_StatusTypeDef ret = i2c::write_byte(COPROCESSOR_ADDR, COPROCESSOR_INIT_BYTE);
        DBG("Coproc init byte sender returns: %u", ret);
        if (ret != HAL_OK) return ret;
        vTaskDelay(pdMS_TO_TICKS(1));

        ret = i2c::read(COPROCESSOR_ADDR, 
            reinterpret_cast<volatile uint8_t*>(filled_buffer), sizeof(memory_map_t));
        DBG("Coprocessor read returns: %u", ret);
        initialized = (ret == HAL_OK);

        return ret;
    }

    uint8_t crc8_avr(uint8_t inCrc, uint8_t inData)
    {
        uint8_t data;
        data = inCrc ^ inData;
        for (size_t i = 0; i < 8; i++ )
        {
            if (( data & 0x80 ) != 0 )
            {
                data <<= 1;
                data ^= 0x07;
            }
            else
            {
                data <<= 1;
            }
        }
        return data;
    }
    void sync()
    {
        if (i2c::read(COPROCESSOR_ADDR, reinterpret_cast<volatile uint8_t*>(standby_buffer), 
            sizeof(memory_map_t)) != HAL_OK) return;

        uint8_t crc = 0xFF;
        // Calculate crc
        for (size_t i = 0; i < (sizeof(memory_map_t) - 1); i++)
        {
            crc = crc8_avr(crc, reinterpret_cast<volatile uint8_t *>(standby_buffer)[i]);
        }
        if (crc == standby_buffer->crc)
        {
            ACQUIRE_MUTEX();
            volatile memory_map_t *p = filled_buffer;
            filled_buffer = standby_buffer;
            standby_buffer = p;
            for (size_t i = 0; i < MY_PUMPS_NUM; i++)
            {
                load_average[i].enqueue(filled_buffer->drv_load[i]);
            }
            RELEASE_MUTEX();
        }
        else
        {
            if (crc_err_stats < UINT32_MAX) crc_err_stats++;
        }
    }
    bool get_initialized()
    {
        return initialized;
    }
    uint32_t get_crc_err_stats()
    {
        return crc_err_stats;
    }

    uint16_t get_encoder_value(size_t i)
    {
        assert_param(i < MAX_ENCODERS);
        assert_param(sync_mutex);

        ACQUIRE_MUTEX();
        uint16_t ret = filled_buffer->encoder_pos[i];
        RELEASE_MUTEX();
        return __UINT16_MAX__ - ret; //Invert to get clockwise = +
    }
    float get_manual_override()
    {
        const float max = 1.0f;
        const float min = 0.01f;
        const float span = 1024.0f * 0.9f;
        assert_param(sync_mutex);

        float res = filled_buffer->manual_override;
        res /= span;
        
        if (res > max) res = max;
        if (res < min) res = min;
        return res;
    }
    bool get_encoder_btn_pressed(size_t i)
    {
        static uint8_t btn_double_buffer = 0;
        assert_param(i < MAX_ENCODERS);

        ACQUIRE_MUTEX();
        btn_double_buffer |= filled_buffer->encoder_btn_pressed;
        filled_buffer->encoder_btn_pressed = 0;
        RELEASE_MUTEX();

        uint8_t mask = (1u << i);
        bool ret = btn_double_buffer & mask;
        btn_double_buffer &= ~mask;

        return ret;
    }
    bool get_drv_error(size_t i)
    {
        assert_param(i < MY_PUMPS_NUM);
        auto copy = filled_buffer->drv_error_bitfield;
        return (copy & (1u << i)) > 0;
    }
    bool get_drv_missing(size_t i)
    {
        assert_param(i < MY_PUMPS_NUM);
        auto copy = filled_buffer->drv_missing_bitfield;
        return (copy & (1u << i)) > 0;
    }
    float get_drv_load(size_t i)
    {
        assert_param(i < MY_PUMPS_NUM);
        ACQUIRE_MUTEX();
        uint8_t copy = load_average[i].get_average();
        RELEASE_MUTEX();
        return static_cast<float>(copy) / 10.0f;
    }
} // namespace coprocessor

_BEGIN_STD_C
STATIC_TASK_BODY(MY_COPROC)
{
    const uint32_t delay = 15;
    static TickType_t last_wake;
    static wdt::task_t* pwdt;

    pwdt = wdt::register_task(500, "coproc");

    if (coprocessor::init() != HAL_OK) { 
        ERR("Failed to init coprocessor task");
        while (1) vTaskDelay(10); 
    } //wdt will reset the controller
    INIT_NOTIFY(MY_COPROC);

    last_wake = xTaskGetTickCount();
    for (;;)
    {
        coprocessor::sync();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(delay));
        pwdt->last_time = xTaskGetTickCount();
        if ((last_wake + pdMS_TO_TICKS(delay)) < pwdt->last_time) last_wake = pwdt->last_time;
    }
}
_END_STD_C