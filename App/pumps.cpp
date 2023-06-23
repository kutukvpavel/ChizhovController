#include "pumps.h"

#include "tim.h"
#include "sr_io.h"
#include "coprocessor.h"

#include <math.h>

namespace pumps
{
    static motor_t* motors[MY_PUMPS_NUM];
    static TIM_HandleTypeDef* m_timers[MY_PUMPS_MAX] = { &htim3, &htim4, &htim10, &htim11 }; 
    static const params_t* params;
    static bool enable = false;
    static float full_assigned_speed[MY_PUMPS_NUM] = { 0 };

    void init(const params_t* p, const motor_params_t* mp, motor_reg_t* mr)
    {
        params = p;
        set_enable(false);
        for (size_t i = 0; i < array_size(motors); i++)
        {
            assert_param(m_timers[i]);
            motors[i] = new motor_t(m_timers[i], static_cast<sr_io::out>(sr_io::out::MOTOR_DIR_0 + i), mp + i, mr + i);
        }
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
            motors[i]->set_volume_rate(full_assigned_speed[i] * coprocessor::get_manual_override());
        }
    }

    void switch_hw_interlock()
    {
        static const sr_io::out motor_enable_interlock = sr_io::out::OC5;

        sr_io::set_output(motor_enable_interlock, true);
        vTaskDelay(pdMS_TO_TICKS(150));
        sr_io::set_output(motor_enable_interlock, false);
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

    bool get_running(size_t i)
    {
        return enable && (!motors[i]->get_paused()) && (motors[i]->get_volume_rate() > __FLT_EPSILON__);
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
        full_assigned_speed[i] = v;
        v *= coprocessor::get_manual_override();
        return motors[i]->set_volume_rate(v);
    }
    HAL_StatusTypeDef increment_speed(size_t i, int32_t diff)
    {
        assert_param(i < MY_PUMPS_NUM);
        if (diff == 0) return HAL_OK;
        return set_indicated_speed(i, full_assigned_speed[i] + params->volume_rate_resolution * diff);
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
} // namespace pumps
