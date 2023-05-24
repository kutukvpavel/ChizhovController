#include "user.h"

#include "sr_io.h"
#include "compat_api.h"
#include "nvs.h"
#include "task_handles.h"

void app_main();

_BEGIN_STD_C

//ADC task
StaticTask_t adc_task_buffer;
StackType_t adc_task_stack[256];
TaskHandle_t adc_task_handle = NULL;
//

void StartMainTask(void *argument)
{
    const uint32_t delay = 1;
    static TickType_t last_wake;

    last_wake = xTaskGetTickCount();
    nvs::init();

    xTaskCreateStatic(StartAdcTask, "adc", array_size(adc_task_stack), NULL, 1, adc_task_stack, &adc_task_buffer);

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

void app_main()
{

}