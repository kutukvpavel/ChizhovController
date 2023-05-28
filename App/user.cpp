#include "user.h"

#include "sr_io.h"
#include "compat_api.h"
#include "nvs.h"
#include "task_handles.h"

#define DEFINE_STATIC_TASK(name, stack_size) \
    StaticTask_t task_buffer_##name; \
    StackType_t task_stack_##name [stack_size]; \
    TaskHandle_t task_handle_##name = NULL
#define START_STATIC_TASK(name, priority) \
    xTaskCreateStatic(start_task_##name, #name, array_size(task_stack_##name), NULL, \
    priority, task_stack_##name, &task_buffer_##name)

void app_main();

DEFINE_STATIC_TASK(MY_ADC, 256);
DEFINE_STATIC_TASK(MY_IO, 256);
DEFINE_STATIC_TASK(MY_ENC, 512);
DEFINE_STATIC_TASK(MY_THERMO, 256);

_BEGIN_STD_C
void StartMainTask(void *argument)
{
    const uint32_t delay = 1;
    static TickType_t last_wake;

    last_wake = xTaskGetTickCount();
    nvs::init();

    START_STATIC_TASK(MY_ADC, 1);
    START_STATIC_TASK(MY_IO, 1);
    START_STATIC_TASK(MY_ENC, 1);
    START_STATIC_TASK(MY_THERMO, 1);

    for (;;)
    {
        app_main();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(delay));
    }
}

void StartThermocoupleTask(void *argument)
{

}

void StartEncoderTask(void *argument)
{

}
_END_STD_C

void app_main()
{

}