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
#include "../USB_DEVICE/App/usb_device.h"
#include "interop.h"

#define DEFINE_STATIC_TASK(name, stack_size) \
    StaticTask_t task_buffer_##name; \
    StackType_t task_stack_##name [stack_size]; \
    TaskHandle_t task_handle_##name = NULL
#define START_STATIC_TASK(name, priority, arg) \
    task_handle_##name = xTaskCreateStatic(start_task_##name, #name, array_size(task_stack_##name), &(arg), \
    priority, task_stack_##name, &task_buffer_##name); \
    ulTaskNotifyTake(pdFALSE, portMAX_DELAY)
#define WAIT_ON_BTN(b) \
    while (front_panel::get_button(b)) \
    {   \
        vTaskDelay(pdMS_TO_TICKS(sr_io::regular_sync_delay_ms)); \
        pwdt->last_time = xTaskGetTickCount(); \
    }

static inline void app_main(wdt::task_t* pwdt);

DEFINE_STATIC_TASK(MY_CLI, 1024);
DEFINE_STATIC_TASK(MY_ADC, 256);
DEFINE_STATIC_TASK(MY_IO, 256);
DEFINE_STATIC_TASK(MY_COPROC, 512);
DEFINE_STATIC_TASK(MY_THERMO, 256);
DEFINE_STATIC_TASK(MY_DISP, 1024);
DEFINE_STATIC_TASK(MY_MODBUS, 1024);
DEFINE_STATIC_TASK(MY_FP, 256);
DEFINE_STATIC_TASK(MY_WDT, 256);

modbusHandler_t modbus;

_BEGIN_STD_C
void StartMainTask(void *argument)
{
    const uint32_t delay = 20;
    static TickType_t last_wake;
    static wdt::task_t* pwdt;
    static TaskHandle_t handle;

    HAL_IWDG_Refresh(&hiwdg);

    handle = xTaskGetCurrentTaskHandle();
    assert_param(handle);
    interop::init();

    START_STATIC_TASK(MY_CLI, 1, handle);
    HAL_IWDG_Refresh(&hiwdg);
    START_STATIC_TASK(MY_WDT, 1, handle);
    HAL_IWDG_Refresh(&hiwdg);
    
    DBG("[@ %lu] Single-threaded init:", compat::micros());
    DBG("I2C Init...");
    i2c::init();
    DBG("SPI Init...");
    spi::init();
    DBG("NVS Init...");
    if (nvs::init() == HAL_OK) nvs::load();
    DBG("Pump Init...");
    pumps::init(nvs::get_pump_params(), nvs::get_motor_params(), nvs::get_motor_regs());
    DBG("USB Init...");
    MX_USB_DEVICE_Init();
    HAL_IWDG_Refresh(&hiwdg);

    DBG("[@ %lu] Staring tasks. Multithreaded init:", compat::micros());
    START_STATIC_TASK(MY_ADC, 1, handle);
    HAL_IWDG_Refresh(&hiwdg);
    START_STATIC_TASK(MY_IO, 1, handle);
    HAL_IWDG_Refresh(&hiwdg);
    START_STATIC_TASK(MY_THERMO, 1, handle);
    HAL_IWDG_Refresh(&hiwdg);
    START_STATIC_TASK(MY_DISP, 1, handle);
    HAL_IWDG_Refresh(&hiwdg);
    START_STATIC_TASK(MY_MODBUS, 1, handle);
    HAL_IWDG_Refresh(&hiwdg);
    START_STATIC_TASK(MY_FP, 1, handle);
    HAL_IWDG_Refresh(&hiwdg);
    START_STATIC_TASK(MY_COPROC, 1, handle);
    
    HAL_IWDG_Refresh(&hiwdg);
    vTaskDelay(pdMS_TO_TICKS(100));

    pwdt = wdt::register_task(2000, "main");
    last_wake = xTaskGetTickCount();
    for (;;)
    {
        app_main(pwdt);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(delay));
        pwdt->last_time = xTaskGetTickCount();
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
    emergency,
    lamp_test
};

void supervize_led(led_states s);
void supervize_fp(states s); //State-independent FP elements
void supervize_manual_mode();

void app_main(wdt::task_t* pwdt)
{
    static const char* state_names[] = {
        "INIT",
        "MANUAL",
        "AUTO",
        "EMERGENCY",
        "LAMP_TEST"
    };
    static led_states led = led_states::INIT;
    static states state = states::init;
    static states prev_state = states::init;

    static_assert(array_size(state_names) >= (static_cast<uint16_t>(states::lamp_test) + 1));

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

    if (prev_state != state)
    {
        size_t idx = static_cast<uint16_t>(state);
        DBG("State = #%u = %s", idx, state_names[idx]);
        prev_state = state;
    }

    switch (state)
    {
    case states::init:
        led = led_states::INIT;
        //Here we wait until the user pushes start button
        front_panel::set_light(front_panel::l_start, front_panel::l_state::blink);
        if (front_panel::get_button(front_panel::b_start))
        {
            front_panel::clear_lights();
            WAIT_ON_BTN(front_panel::b_start);
            pumps::switch_hw_interlock();
            state = states::manual;
        }
        if (front_panel::get_button(front_panel::b_light_test))
        {
            front_panel::clear_lights();
            //WAIT_ON_BTN(front_panel::b_light_test);
            state = states::lamp_test;
        }
        break;

    case states::manual:
        led = led_states::HEARTBEAT;
        front_panel::set_light(front_panel::l_start, front_panel::l_state::on);
        supervize_manual_mode();
        if (front_panel::get_button(front_panel::b_start))
        {
            front_panel::clear_lights();
            WAIT_ON_BTN(front_panel::b_start);
            state = states::automatic;
        }
        if (front_panel::get_button(front_panel::b_stop))
        {
            front_panel::clear_lights();
            WAIT_ON_BTN(front_panel::b_stop);
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
            WAIT_ON_BTN(front_panel::b_stop);
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
            WAIT_ON_BTN(front_panel::b_stop);
            state = states::init;
        }
        break;

    case states::lamp_test:
    {
        static display::test_modes mode = display::test_modes::all_lit;

        led = led_states::COMMUNICATION;
        front_panel::test();
        display::set_lamp_test_mode(mode);
        if (!front_panel::get_button(front_panel::b_light_test))
        {
            front_panel::clear_lights();
            //WAIT_ON_BTN(front_panel::b_light_test);
            mode = static_cast<display::test_modes>((mode + 1) % display::test_modes::TST_LEN);
            if (mode == display::test_modes::none) mode = display::test_modes::all_lit;
            display::set_lamp_test_mode(display::test_modes::none);
            DBG("Next LT mode = #%u", mode);
            vTaskDelay(pdMS_TO_TICKS(50));
            state = states::init;
        }
        break;
    }
    
    default:
        DBG("State machine corrupt");
        vTaskDelay(pdMS_TO_TICKS(100));
        HAL_NVIC_SystemReset();
        break;
    }
    if ((!pumps::get_hw_interlock_ok()) && (state != states::init) && (state != states::lamp_test) && (state != states::emergency)) 
    {
        front_panel::clear_lights();
        state = states::emergency;
    }
    pumps::set_enable(state == states::automatic || state == states::manual);
    mb_regs::set_remote(state == states::automatic);
    mb_regs::set_status(static_cast<uint16_t>(state));

    if (state != states::lamp_test) supervize_fp(state);
    supervize_led(led);
}

void supervize_fp(states s)
{
    front_panel::set_light(front_panel::l_manual_override, 
        (coprocessor::get_manual_override() < 0.999f) ? front_panel::l_state::on : front_panel::l_state::off);
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
    static const TickType_t max_edit_delay = pdMS_TO_TICKS(5000);
    static edit_t enable_edit[MY_PUMPS_NUM] = { };

    if (!coprocessor::get_initialized()) return;

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
    const uint32_t comm_delay_ms = 200;
    const uint32_t comm_repetitions = 3;

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