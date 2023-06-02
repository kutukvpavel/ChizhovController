#include "user.h"

#include "compat_api.h"
#include "nvs.h"
#include "task_handles.h"
#include "modbus/MODBUS-LIB/Inc/Modbus.h"
#include "wdt.h"
#include "i2c_sync.h"
#include "spi_sync.h"
#include "pumps.h"
#include "../Core/Inc/iwdg.h"
#include "modbus_regs.h"
#include "display.h"
#include "front_panel.h"
#include "coprocessor.h"

#define DEFINE_STATIC_TASK(name, stack_size) \
    StaticTask_t task_buffer_##name; \
    StackType_t task_stack_##name [stack_size]; \
    TaskHandle_t task_handle_##name = NULL
#define START_STATIC_TASK(name, priority) \
    xTaskCreateStatic(start_task_##name, #name, array_size(task_stack_##name), NULL, \
    priority, task_stack_##name, &task_buffer_##name)

static inline void app_main();

DEFINE_STATIC_TASK(MY_CLI, 1024);
DEFINE_STATIC_TASK(MY_ADC, 256);
DEFINE_STATIC_TASK(MY_IO, 256);
DEFINE_STATIC_TASK(MY_COPROC, 512);
DEFINE_STATIC_TASK(MY_THERMO, 256);
DEFINE_STATIC_TASK(MY_DISP, 1024);
DEFINE_STATIC_TASK(MY_MODBUS, 1024);
DEFINE_STATIC_TASK(MY_FP, 256);

modbusHandler_t modbus;

_BEGIN_STD_C
void StartMainTask(void *argument)
{
    const uint32_t delay = 10;
    static TickType_t last_wake;
    static wdt::task_t* pwdt;
    
    HAL_IWDG_Refresh(&hiwdg);
    vTaskDelay(pdMS_TO_TICKS(100)); //For the coprocessor to initialize

    pwdt = wdt::register_task(500);
    i2c::init();
    spi::init();
    if (nvs::init() == HAL_OK) nvs::load();
    pumps::init(nvs::get_pump_params(), nvs::get_motor_params(), nvs::get_motor_regs());

    START_STATIC_TASK(MY_CLI, 1);
    START_STATIC_TASK(MY_ADC, 1);
    START_STATIC_TASK(MY_IO, 1);
    START_STATIC_TASK(MY_COPROC, 1);
    START_STATIC_TASK(MY_THERMO, 1);
    START_STATIC_TASK(MY_DISP, 1);
    START_STATIC_TASK(MY_MODBUS, 1);
    START_STATIC_TASK(MY_FP, 1);

    HAL_IWDG_Refresh(&hiwdg);
    last_wake = xTaskGetTickCount();
    for (;;)
    {
        app_main();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(delay));
        pwdt->last_time = last_wake;
    }
}
_END_STD_C

enum class led_states
{
    HEARTBEAT,
    ERROR,
    COMMUNICATION,
    INIT
};
enum class states : uint16_t
{
    init,
    manual,
    automatic,
    emergency
};

void supervize_led(led_states s);
void supervize_fp(states s); //State-independent FP elements
void supervize_manual_mode();

void app_main()
{
    static led_states led = led_states::INIT;
    static states state = states::init;

    /***
     * The following stuff is handled in separate tasks:
     *  debug CLI
     *  STM32 ADC reading
     *  SR IO sync
     *  Coprocessor poll
     *  MAX6675 poll
     *  Display update
     *  Modbus requests
     *  Modbus buffer sync
     * 
     * The following remains for the main task to handle:
     *  Status LED
     *  Manual pump speed update
     *  State machine
    */

    switch (state)
    {
    case states::init:
        led = led_states::INIT;
        //Here we wait until the user pushes start button
        front_panel::set_light(front_panel::l_start, front_panel::l_state::blink);
        if (front_panel::get_button(front_panel::b_start))
        {
            front_panel::clear_lights();
            while (front_panel::get_button(front_panel::b_start)) vTaskDelay(pdMS_TO_TICKS(sr_io::regular_sync_delay_ms));
            state = states::manual;
        }
        break;

    case states::manual:
        led = led_states::HEARTBEAT;
        front_panel::set_light(front_panel::l_start, front_panel::l_state::on);
        supervize_manual_mode();
        if (front_panel::get_button(front_panel::b_start))
        {
            front_panel::clear_lights();
            while (front_panel::get_button(front_panel::b_start)) vTaskDelay(pdMS_TO_TICKS(sr_io::regular_sync_delay_ms));
            state = states::automatic;
        }
        if (front_panel::get_button(front_panel::b_stop))
        {
            front_panel::clear_lights();
            while (front_panel::get_button(front_panel::b_stop)) vTaskDelay(pdMS_TO_TICKS(sr_io::regular_sync_delay_ms));
            state = states::init;
        }
        break;

    case states::automatic:
        led = led_states::COMMUNICATION;
        front_panel::set_light(front_panel::l_automatic_mode, front_panel::l_state::on);
        front_panel::set_light(front_panel::l_stop, front_panel::l_state::on);
        if (front_panel::get_button(front_panel::b_stop))
        {
            front_panel::clear_lights();
            while (front_panel::get_button(front_panel::b_stop)) vTaskDelay(pdMS_TO_TICKS(sr_io::regular_sync_delay_ms));
            state = states::manual;
        }
        break;

    case states::emergency:
        led = led_states::ERROR;
        front_panel::set_light(front_panel::l_emergency, front_panel::l_state::on);
        front_panel::set_light(front_panel::l_stop, front_panel::l_state::blink);
        if (front_panel::get_button(front_panel::b_stop))
        {
            front_panel::clear_lights();
            state = states::init;
        }
        break;
    
    default:
        HAL_NVIC_SystemReset();
        break;
    }
    if (!front_panel::get_button(front_panel::b_emergency)) state = states::emergency;
    pumps::set_enable(state == states::automatic || state == states::manual);
    mb_regs::set_remote(state == states::automatic);
    mb_regs::set_status(static_cast<uint16_t>(state));

    supervize_fp(state);
    supervize_led(led);
}

void supervize_fp(states s)
{
    front_panel::set_light(front_panel::l_manual_override, 
        (coprocessor::get_manual_override() < 0.999f) ? front_panel::l_state::blink : front_panel::l_state::off);
    for (size_t i = 0; i < MY_PUMPS_NUM; i++)
    {
        pumps::set_paused(i, front_panel::get_button(front_panel::b_pause, i));
    }
}

void supervize_manual_mode()
{
    struct edit_t
    {
        bool enable = false;
        TickType_t time = configINITIAL_TICK_COUNT;
        uint32_t last_pos = 0;
    };
    const TickType_t max_edit_delay = pdMS_TO_TICKS(5000);
    edit_t enable_edit[MY_PUMPS_NUM] = { };

    TickType_t now = xTaskGetTickCount();
    for (size_t i = 0; i < MY_PUMPS_NUM; i++)
    {
        auto& e = enable_edit[i];
        if (coprocessor::get_encoder_btn_pressed(i)) 
        {
            e.enable = !e.enable;
            e.time = now;
        }
        if (e.enable && ((now - e.time) > max_edit_delay)) e.enable = false;
        if (!e.enable) continue;
        uint32_t current_pos = coprocessor::get_encoder_value(i);
        pumps::increment_speed(i, static_cast<int32_t>(
            static_cast<int64_t>(current_pos) - static_cast<int64_t>(e.last_pos)));
        e.last_pos = current_pos;
    }
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