#pragma once

#include "user.h"
#include "tim.h"
#include "sr_io.h"

struct PACKED_FOR_MODBUS motor_params_t
{
    uint16_t dir;
    uint16_t microsteps;
    float volume_rate_to_rps;
};

class motor_t
{
private:
    TIM_HandleTypeDef* timer;
    sr_io::out pin_dir;
public:
    motor_t(TIM_HandleTypeDef* tim, sr_io::out dir);
    ~motor_t();

    void set_volume_rate(float v);
    float get_volume_rate();
};
