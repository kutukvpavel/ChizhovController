#include "pumps.h"s

#include "tim.h"
#include "sr_io.h"

namespace pumps
{
    motor_t motors[MY_PUMPS_NUM] = {
        { &htim3, sr_io::out::MOTOR_DIR_0 },
        { &htim4, sr_io::out::MOTOR_DIR_1 },
        { &htim10, sr_io::out::MOTOR_DIR_2 },
        { &htim11, sr_io::out::MOTOR_DIR_3 }
    };
    const params_t* params;

    void init(const params_t* p)
    {
        params = p;
    }
    void set_enable(bool v)
    {
        sr_io::set_output(sr_io::out::MOTORS_EN, params->invert_enable ? !v : v);
    }
    float get_indicated_speed(size_t i)
    {
        assert_param(i < MY_PUMPS_NUM);
        return motors[i].get_volume_rate();
    }
    void set_indicated_speed(float v)
    {

    }
} // namespace pumps
