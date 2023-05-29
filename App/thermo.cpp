#include "thermo.h"

#include "task_handles.h"
#include "wdt.h"

#include <math.h>

namespace thermo
{
    float channels[MY_TEMP_CHANNEL_NUM] = { NAN, NAN };

    void init()
    {

    }

    void sync()
    {
        
    }

    float get_temperature(size_t i)
    {

    }
} // namespace thermo

_BEGIN_STD_C
STATIC_TASK_BODY(MY_THERMO)
{
    const uint32_t delay = 50;
    static TickType_t last_wake;
    static wdt::task_t* pwdt;

    pwdt = wdt::register_task(500);

    thermo::init();

    for (;;)
    {
        thermo::sync();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(delay));
        pwdt->last_time = last_wake;
    }
}
_END_STD_C