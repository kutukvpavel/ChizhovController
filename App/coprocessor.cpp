#include "coprocessor.h"

#include "task_handles.h"
#include "wdt.h"
#include "i2c_sync.h"

#define MAX_ENCODERS 3
#define COPROCESSOR_ADDR 0x08
#define PACKED_FOR_I2C __packed __aligned(sizeof(uint32_t))
#define RETRIES 3

#define ACQUIRE_MUTEX() while (xSemaphoreTake(sync_mutex, portMAX_DELAY) != pdTRUE)
#define RELEASE_MUTEX() xSemaphoreGive(sync_mutex)

namespace coprocessor
{
    struct PACKED_FOR_I2C memory_map_t
    {
        uint32_t encoder_pos[MAX_ENCODERS];
        uint8_t encoder_btn_pressed;
        uint16_t manual_override;
        uint8_t drv_error_bitfield;
        uint8_t drv_missing_bitfield;
    };
    static memory_map_t buffer = {};
    static SemaphoreHandle_t sync_mutex = NULL;

    HAL_StatusTypeDef init()
    {
        sync_mutex = xSemaphoreCreateMutex();
        return sync_mutex ? HAL_OK : HAL_ERROR;
    }

    void sync()
    {
        if (xSemaphoreTake(sync_mutex, pdMS_TO_TICKS(10)) != pdTRUE) return;

        int retries = RETRIES;
        while (retries--)
        {
            if (i2c::read(COPROCESSOR_ADDR, reinterpret_cast<uint8_t*>(&buffer), sizeof(buffer)) == HAL_OK) break;
        }
        RELEASE_MUTEX();
    }

    uint32_t get_encoder_value(size_t i)
    {
        assert_param(i < MAX_ENCODERS);
        assert_param(sync_mutex);

        ACQUIRE_MUTEX();
        uint32_t ret = buffer.encoder_pos[i];

        RELEASE_MUTEX();
        return ret;
    }
    float get_manual_override()
    {
        const float max = 1.0f;
        const float min = 0.01f;
        const float span = 1024.0f * 0.9f;
        assert_param(sync_mutex);

        ACQUIRE_MUTEX();
        float res = buffer.manual_override / span;
        RELEASE_MUTEX();
        
        if (res > max) res = max;
        if (res < min) res = min;
        return res;
    }
    bool get_encoder_btn_pressed(size_t i)
    {
        static uint8_t btn_double_buffer = 0;
        assert_param(i < MAX_ENCODERS);

        ACQUIRE_MUTEX();
        btn_double_buffer |= buffer.encoder_btn_pressed;
        buffer.encoder_btn_pressed = 0;
        uint8_t mask = (1u << i);
        bool ret = btn_double_buffer & mask;
        btn_double_buffer &= ~mask;

        RELEASE_MUTEX();
        return ret;
    }
} // namespace coprocessor

_BEGIN_STD_C
STATIC_TASK_BODY(MY_COPROC)
{
    const uint32_t delay = 15;
    static TickType_t last_wake;
    static wdt::task_t* pwdt;

    pwdt = wdt::register_task(500);

    if (coprocessor::init() != HAL_OK) while (1); //wdt will reset the controller

    for (;;)
    {
        coprocessor::sync();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(delay));
        pwdt->last_time = last_wake;
    }
}
_END_STD_C