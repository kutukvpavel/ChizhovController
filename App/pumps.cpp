#include "pumps.h"

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
} // namespace pumps
