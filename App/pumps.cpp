#include "pumps.h"

#include "tim.h"
#include "sr_io.h"

namespace pumps
{
    static motor_t* motors[MY_PUMPS_NUM];
    static TIM_HandleTypeDef* m_timers[MY_PUMPS_NUM] = { &htim3, &htim4, &htim10, &htim11 }; 
    static const params_t* params;
    static bool enable = false;

    void init(const params_t* p, const motor_params_t* mp, motor_reg_t* mr)
    {
        params = p;
        for (size_t i = 0; i < array_size(motors); i++)
        {
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

    void set_enable(bool v)
    {
        sr_io::set_output(sr_io::out::MOTORS_EN, params->invert_enable ? !v : v);
        enable = v;
    }
    float get_indicated_speed(size_t i)
    {
        assert_param(i < MY_PUMPS_NUM);
        return motors[i]->get_volume_rate();
    }
    void set_indicated_speed(size_t i, float v)
    {
        assert_param(i < MY_PUMPS_NUM);
        motors[i]->set_volume_rate(v);
    }
    float get_speed_fraction(size_t i)
    {
        return motors[i]->get_speed_fraction();
    }
    float get_load_fraction(size_t i)
    {
        return motors[i]->get_load_fraction();
    }
    bool get_missing(size_t i)
    {
        return motors[i]->get_missing();
    }
    void set_missing(size_t i, bool v)
    {
        motors[i]->set_missing(v);
    }
    bool get_overload(size_t i)
    {
        return motors[i]->get_overload();
    }
    void set_overload(size_t i, bool v)
    {
        motors[i]->set_overload(v);
    }
} // namespace pumps
