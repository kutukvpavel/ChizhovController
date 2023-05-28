#include "user.h"

#include "sr_io.h"
#include "compat_api.h"
#include "nvs.h"
#include "task_handles.h"
#include "modbus/MODBUS-LIB/Inc/Modbus.h"

#define DEFINE_STATIC_TASK(name, stack_size) \
    StaticTask_t task_buffer_##name; \
    StackType_t task_stack_##name [stack_size]; \
    TaskHandle_t task_handle_##name = NULL
#define START_STATIC_TASK(name, priority) \
    xTaskCreateStatic(start_task_##name, #name, array_size(task_stack_##name), NULL, \
    priority, task_stack_##name, &task_buffer_##name)

void app_main();

DEFINE_STATIC_TASK(MY_CLI, 1024);
DEFINE_STATIC_TASK(MY_ADC, 256);
DEFINE_STATIC_TASK(MY_IO, 256);
DEFINE_STATIC_TASK(MY_ENC, 512);
DEFINE_STATIC_TASK(MY_THERMO, 256);
DEFINE_STATIC_TASK(MY_DISP, 1024);

modbusHandler_t modbus;

_BEGIN_STD_C
void StartMainTask(void *argument)
{
    const uint32_t delay = 10;
    static TickType_t last_wake;

    nvs::init();
    ModbusInit(&modbus);

    START_STATIC_TASK(MY_CLI, 1);
    START_STATIC_TASK(MY_ADC, 1);
    START_STATIC_TASK(MY_IO, 1);
    START_STATIC_TASK(MY_ENC, 1);
    START_STATIC_TASK(MY_THERMO, 1);
    START_STATIC_TASK(MY_DISP, 1);

    ModbusStartCDC(&modbus);

    last_wake = xTaskGetTickCount();
    for (;;)
    {
        app_main();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(delay));
    }
}

STATIC_TASK_BODY(MY_THERMO)
{

}

STATIC_TASK_BODY(MY_ENC)
{

}
_END_STD_C

enum class led_states
{
    HEARTBEAT,
    ERROR,
    COMMUNICATION,
    INIT
};
void supervize_led(led_states s);

void app_main()
{
    static led_states led = led_states::HEARTBEAT;

    supervize_led(led);
}

void supervize_led(led_states s)
{
    enum class substates
    {
        on, off, blinking
    };

    const uint32_t heartbeat_delay_ms = 1000;
    const uint32_t comm_delay_ms = 100;
    const uint32_t comm_repetitions = 5;

    static TickType_t next_toggle = 0;
    static substates substate = substates::off;
    static int next_substate_switch = 0;
    static led_states prev_state = led_states::INIT;
    static TickType_t current_toggle_period = 0;

    bool state_change = (prev_state != s);
    TickType_t tick = xTaskGetTickCount();

    if ((tick < next_toggle) && !state_change) return;
    switch (substate)
    {
    case substates::blinking:
        HAL_GPIO_TogglePin(Onboard_LED_GPIO_Port, Onboard_LED_Pin);
        next_toggle = tick + current_toggle_period;
        break;
    case substates::on:
        LL_GPIO_SetOutputPin(Onboard_LED_GPIO_Port, Onboard_LED_Pin);
        break;
    default:
        LL_GPIO_ResetOutputPin(Onboard_LED_GPIO_Port, Onboard_LED_Pin);
        break;
    }

    if ((--next_substate_switch > 0) && !state_change) return;
    if (next_substate_switch < 0) next_substate_switch = 0;
    switch (s)
    {
    case led_states::HEARTBEAT:
        substate = substates::blinking;
        current_toggle_period = heartbeat_delay_ms;
        break;
    case led_states::ERROR:
        substate = substates::on;
        break;
    case led_states::COMMUNICATION:
        current_toggle_period = comm_delay_ms;
        next_substate_switch = comm_repetitions + 1;
        if (substate == substates::blinking)
        {
            substate = substates::off;
        }
        else
        {
            substate = substates::blinking;
        }
        break;
    case led_states::INIT:
        substate = substates::off;
    default:
        break;
    }
}