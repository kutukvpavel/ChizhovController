#include "front_panel.h"

#include "sr_io.h"
#include "task_handles.h"
#include "pumps.h"
#include "compat_api.h"

namespace front_panel
{
    static const sr_io::out lights_mapping[lights::L_LEN] = {
        sr_io::out::OC1,
        sr_io::out::OC2,
        sr_io::out::OC3,
        sr_io::out::OC4,
        sr_io::out::OC5
    };
    static const sr_io::in buttons_mapping[buttons::B_LEN] = {
        sr_io::in::IN4,
        sr_io::in::IN5,
        sr_io::in::IN6,
        sr_io::in::IN7,
        sr_io::in::IN8,
    };
    static const sr_io::in pause_btn_mapping[MY_PUMPS_MAX] = {
        sr_io::in::IN0,
        sr_io::in::IN1,
        sr_io::in::IN2,
        sr_io::in::IN3
    };

    static bool blink_enable[lights::L_LEN] = { 0 };
    static bool blink_state[lights::L_LEN];
    static SemaphoreHandle_t blink_mutex = NULL;

    void init()
    {
        DBG("Front panel init...");
        blink_mutex = xSemaphoreCreateMutex();
        assert_param(blink_mutex);
    }

    void clear_lights()
    {
        while (xSemaphoreTake(blink_mutex, portMAX_DELAY) != pdTRUE);
        for (size_t i = 0; i < lights::L_LEN; i++)
        {
            blink_enable[i] = false; 
            sr_io::set_output(lights_mapping[i], false);
        }
        xSemaphoreGive(blink_mutex);
    }
    void test()
    {
        while (xSemaphoreTake(blink_mutex, portMAX_DELAY) != pdTRUE);
        for (size_t i = 0; i < lights::L_LEN; i++)
        {
            blink_enable[i] = false;
            sr_io::set_output(lights_mapping[i], true);
        }
        xSemaphoreGive(blink_mutex);
    }
    void set_light(lights l, l_state on, size_t channel)
    {
        while (xSemaphoreTake(blink_mutex, portMAX_DELAY) != pdTRUE);
        blink_enable[l] = (on == l_state::blink);
        if (!blink_enable[l]) sr_io::set_output(lights_mapping[l], (on == l_state::on) ? true : false);
        xSemaphoreGive(blink_mutex);
    }
    bool get_button(buttons b, size_t channel)
    {
        struct debouncer_t
        {
            int32_t result = -1;
            bool prev = false;
            uint32_t counter = 0;
        };
        const uint32_t delay = 2;
        static debouncer_t debounce[buttons::B_LEN] = {};
        static debouncer_t debounce_pause[MY_PUMPS_NUM] = {};

        bool current;
        debouncer_t* d;
        switch (b)
        {
        case buttons::b_pause:
            current = sr_io::get_input(pause_btn_mapping[channel]);
            d = &(debounce_pause[channel]);
        
        default:
            current = sr_io::get_input(buttons_mapping[b]);
            d = &(debounce[b]);
        }

        if (d->result < 0) //First time init
        {
            d->result = current ? 1 : 0;
            d->prev = d->result;
        }
        if (d->prev == current) d->counter++;
        else
        {
            d->counter = 0;
            d->prev = current;
        }
        if (d->counter > delay)
        {
            d->result = current;
            d->counter = 0;
        }
        return d->result;
    }
} // namespace front_panel

_BEGIN_STD_C
STATIC_TASK_BODY(MY_FP)
{
    const uint32_t regular_delay = 1000; //ms
    const uint32_t missed_delay = 100;
    static uint32_t delay = regular_delay;

    front_panel::init();

    for (;;)
    {
        vTaskDelay(pdMS_TO_TICKS(delay));
        if (xSemaphoreTake(front_panel::blink_mutex, pdMS_TO_TICKS(50)) != pdTRUE)
        {
            delay = missed_delay;
            continue;
        }
        else
        {
            delay = regular_delay;
        }
        for (size_t i = 0; i < front_panel::lights::L_LEN; i++)
        {
            if (!front_panel::blink_enable[i]) continue;
            front_panel::blink_state[i] = !front_panel::blink_state[i];
            sr_io::set_output(front_panel::lights_mapping[i], front_panel::blink_state[i]);
        }
        xSemaphoreGive(front_panel::blink_mutex);
    }
}
_END_STD_C