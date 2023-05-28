#pragma once

#include "user.h"
#include "motor.h"

#define MY_PUMPS_NUM 4

namespace pumps
{
    struct PACKED_FOR_MODBUS params_t
    {
        uint16_t invert_enable;
    };

    //extern motor_t motors[MY_PUMPS_NUM];

    void init(const params_t* p, const motor_params_t** mp, motor_reg_t** mr);
    void set_enable(bool v);
    float get_indicated_speed(size_t i);
    float get_speed_fraction(size_t i);
    float get_load_fraction(size_t i);
} // namespace pumps
