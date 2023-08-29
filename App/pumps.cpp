#include "pumps.h"

#include "tim.h"
#include "sr_io.h"
#include "coprocessor.h"
#include "task_handles.h"
#include "wdt.h"

#include "../Middlewares/Third_Party/FreeRTOS/Source/include/timers.h"
#include <math.h>

namespace pumps
{
    struct instance_cfg_t
    {
        TIM_HandleTypeDef* timer;
        uint32_t timer_channel;
        float full_assigned_speed;
    };

    static motor_t* motors[MY_PUMPS_NUM];
    static instance_cfg_t configs[MY_PUMPS_MAX] = {
        {
            &htim11,
            TIM_CHANNEL_1,
            0
        },
        {
            &htim10,
            TIM_CHANNEL_1,
            0
        },
        {
            &htim4,
            TIM_CHANNEL_2,
            0
        }, 
        {
            &htim3,
            TIM_CHANNEL_2,
            0
        }
    };
    static const params_t* params;
    static bool enable = false;
    static uint32_t consistent_overload_counter[MY_PUMPS_MAX] = {};
    static motor_reg_t* motor_regs;
    static SemaphoreHandle_t tick_mutex = NULL;
    static uint32_t deadlock_counter = 0;
    static TimerHandle_t tick_10ms_timer;

    void tick_10ms(TimerHandle_t t);

    void init(const params_t* p, const motor_params_t* mp, motor_reg_t* mr)
    {
        assert_param(p);
        assert_param(mp);
        assert_param(mr);
        
        motor_regs = mr;
        params = p;
        set_enable(false);
        for (size_t i = 0; i < array_size(motors); i++)
        {
            auto& cfg = configs[i];
            assert_param(cfg.timer);
            motors[i] = new motor_t(cfg.timer, cfg.timer_channel, 
                static_cast<sr_io::out>(sr_io::out::MOTOR_DIR_0 + i), mp + i, mr + i);
            cfg.full_assigned_speed = mr[i].volume_rate;
        }
        tick_mutex = xSemaphoreCreateMutex();
        assert_param(tick_mutex);
        tick_10ms_timer = xTimerCreate("PUMP", pdMS_TO_TICKS(10), pdTRUE, tick_10ms_timer, tick_10ms);
    }

    void reload_motor_params()
    {
        for (auto i : motors)
        {
            i->reload_params();
        }
    }
    void reload_params()
    {
        set_enable(enable);
    }
    void update_manual_override()
    {
        for (size_t i = 0; i < MY_PUMPS_NUM; i++)
        {
            motors[i]->set_volume_rate(configs[i].full_assigned_speed * coprocessor::get_manual_override());
        }
    }
    void print_debug_info()
    {
        printf("Tick deadlock stats: %lu\n", deadlock_counter);
        for (size_t i = 0; i < MY_PUMPS_NUM; i++)
        {
            printf("Motor #%u:\n", i);
            motors[i]->print_debug_info();
        }
    }
    void update_load()
    {
        for (size_t i = 0; i < MY_PUMPS_NUM; i++)
        {
            motors[i]->set_load_err(coprocessor::get_drv_load(i));
        }
    }

    void tick_10ms(TimerHandle_t t)
    {
        if (xSemaphoreTake(tick_mutex, pdMS_TO_TICKS(5)) != pdTRUE)
        {
            ERR("Tick locked");
            deadlock_counter++;
            return;
        }
        for (size_t i = 0; i < MY_PUMPS_NUM; i++)
        {
            auto& r = motor_regs[i];
            auto& m = motors[i];
            if (!m->check_status_bit(motor_t::status_bits::on_timer))
            {
                m->set_disabled_by_timer(false);
                continue;
            }
            if (r.time_left <= 0) continue;
            if (motors[i]->get_paused()) continue;

            r.time_left -= 0.010f;
            m->set_disabled_by_timer(r.time_left <= 0);
        }
        xSemaphoreGive(tick_mutex);
    }
    void enable_hw_interlock(bool v)
    {
        static const sr_io::out motor_enable_interlock = sr_io::out::OC6;

        sr_io::set_output(motor_enable_interlock, v);
    }
    void switch_hw_interlock()
    {
        static const sr_io::out motor_pulse_interlock = sr_io::out::OC5;

        sr_io::set_output(motor_pulse_interlock, true);
        vTaskDelay(pdMS_TO_TICKS(150));
        sr_io::set_output(motor_pulse_interlock, false);
    }
    bool get_hw_interlock_ok()
    {
        static const sr_io::in motor_power_sense = sr_io::in::IN0;

        return sr_io::get_input(motor_power_sense);
    }
    void set_enable(bool v)
    {
        sr_io::set_output(sr_io::out::MOTORS_EN, params->invert_enable ? !v : v);
        enable = v;
    }
    bool get_enabled()
    {
        return enable;
    }

    void set_timer(size_t i, float t)
    {
        assert_param(i < MY_PUMPS_NUM);

        if (isfinite(t))
        {
            while (xSemaphoreTake(tick_mutex, portMAX_DELAY) != pdTRUE);
            motor_regs[i].time_left = t;
            motors[i]->set_status_bit(motor_t::status_bits::on_timer, true);
            xSemaphoreGive(tick_mutex);
        }
        else
        {
            motors[i]->set_status_bit(motor_t::status_bits::on_timer, false);
        }
    }
    bool get_running(size_t i)
    {
        return enable && (!motors[i]->get_paused()) && (motors[i]->get_volume_rate() > __FLT_EPSILON__)
            && (!motors[i]->get_missing());
    }
    float get_indicated_speed(size_t i)
    {
        assert_param(i < MY_PUMPS_NUM);
        return motors[i]->get_volume_rate();
    }
    HAL_StatusTypeDef set_indicated_speed(size_t i, float v)
    {
        assert_param(i < MY_PUMPS_NUM);
        assert_param(v >= 0);
        if (v < 0) v = 0;
        float lim = motors[i]->get_volume_rate_limit();
        if (v > lim) v = lim;
        configs[i].full_assigned_speed = v;
        v *= coprocessor::get_manual_override();
        return motors[i]->set_volume_rate(v);
    }
    HAL_StatusTypeDef increment_speed(size_t i, int32_t diff)
    {
        assert_param(i < MY_PUMPS_NUM);
        if (diff == 0) return HAL_OK;
        return set_indicated_speed(i, configs[i].full_assigned_speed + params->volume_rate_resolution * diff);
    }
    float get_speed_fraction(size_t i)
    {
        assert_param(i < MY_PUMPS_NUM);
        return motors[i]->get_speed_fraction();
    }
    float get_load_fraction(size_t i)
    {
        assert_param(i < MY_PUMPS_NUM);
        return motors[i]->get_load_fraction();
    }
    bool get_missing(size_t i)
    {
        assert_param(i < MY_PUMPS_NUM);
        return motors[i]->get_missing();
    }
    void set_missing(size_t i, bool v)
    {
        assert_param(i < MY_PUMPS_NUM);
        motors[i]->set_missing(v);
    }
    bool get_overload(size_t i)
    {
        assert_param(i < MY_PUMPS_NUM);
        return motors[i]->get_overload();
    }
    bool get_paused(size_t i)
    {
        assert_param(i < MY_PUMPS_NUM);
        return motors[i]->get_paused();
    }
    void set_paused(size_t i, bool v)
    {
        assert_param(i < MY_PUMPS_NUM);
        motors[i]->set_paused(v);
    }
    bool get_consistent_overload()
    {
        const uint32_t threshold = 10;
        const uint32_t delay_ticks = pdMS_TO_TICKS(100);
        static TickType_t last = configINITIAL_TICK_COUNT;

        TickType_t now = xTaskGetTickCount();
        bool res = false;
        if ((now - last) > delay_ticks)
        {
            last = now;
            for (size_t i = 0; i < MY_PUMPS_NUM; i++)
            {
                if (motors[i]->get_overload()) consistent_overload_counter[i]++;
                else consistent_overload_counter[i] = 0;
                if (consistent_overload_counter[i] > threshold) res = true;
            }
        }
        return res;
    }
    void reset_consistent_overload()
    {
        for (size_t i = 0; i < MY_PUMPS_NUM; i++)
        {
            consistent_overload_counter[i] = 0;
        }
    }
    void reload_motor_regs()
    {
        for (size_t i = 0; i < MY_PUMPS_NUM; i++)
        {
            set_indicated_speed(i, motor_regs[i].volume_rate);    
        }
    }
    void stop_all()
    {
        for (size_t i = 0; i < MY_PUMPS_NUM; i++)
        {
            set_indicated_speed(i, 0);
        }
    }
} // namespace pumps

/*_BEGIN_STD_C
STATIC_TASK_BODY(MY_PUMP)
{
    static const TickType_t interval = pdMS_TO_TICKS(10);
    TickType_t last_wake = configINITIAL_TICK_COUNT;
    
    auto pwdt = wdt::register_task(200, "PUMPS");
    last_wake = xTaskGetTickCount();

    for (;;)
    {
        pumps::tick_10ms();

        vTaskDelayUntil(&last_wake, interval);
        pwdt->last_time = xTaskGetTickCount();
        if ((last_wake + interval) < pwdt->last_time) 
        {
            last_wake = pwdt->last_time;
            pumps::deadlock_counter++;
            ERR("Realtime delay!");
        }
    }
}
_END_STD_C*/