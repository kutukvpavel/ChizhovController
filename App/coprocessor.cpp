#include "coprocessor.h"

#include "task_handles.h"
#include "wdt.h"

#define MAX_ENCODERS 3

namespace coprocessor
{
    struct memory_map_t
    {
        uint32_t encoder_pos[MAX_ENCODERS];
        uint16_t manual_override;
        uint8_t drv_error_bitfield;
        uint8_t drv_missing_bitfield;
    };
    memory_map_t buffer = {};

    void init()
    {

    }

    void sync()
    {

    }

    uint32_t get_encoder_value(size_t i)
    {
        assert_param(i < MAX_ENCODERS);

        return buffer.encoder_pos[i];
    }
} // namespace coprocessor

_BEGIN_STD_C
STATIC_TASK_BODY(MY_COPROC)
{
    const uint32_t delay = 15;
    static TickType_t last_wake;
    static wdt::task_t* pwdt;

    pwdt = wdt::register_task(500);

    coprocessor::init();

    for (;;)
    {
        coprocessor::sync();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(delay));
        pwdt->last_time = last_wake;
    }
}
_END_STD_C