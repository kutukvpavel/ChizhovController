#include "user.h"

#include "sr_io.h"
#include "compat_api.h"

_BEGIN_STD_C

void StartMainTask(void *argument)
{
    const uint32_t delay = 1;
    static TickType_t last_wake;

    last_wake = xTaskGetTickCount();

    for (;;)
    {
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(delay));
    }
}

void StartThermocoupleTask(void *argument)
{

}

void StartEncoderTask(void *argument)
{

}

void StartUsbTask(void *argument)
{

}

void StartRs485Task(void *argument)
{

}

void StartEthernetTask(void *argument)
{

}

_END_STD_C