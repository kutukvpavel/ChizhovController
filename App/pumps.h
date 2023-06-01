#pragma once

#include "user.h"
#include "motor.h"

#define MY_PUMPS_NUM 4

namespace pumps
{
    struct PACKED_FOR_MODBUS params_t
    {
        uint16_t invert_enable;
        uint16_t reserved; //Alignment
    };

    //extern motor_t motors[MY_PUMPS_NUM];

    void init(const params_t* p, const motor_params_t* mp, motor_reg_t* mr);

    void reload_motor_params();
    void reload_params();

    void set_enable(bool v);
    float get_indicated_speed(size_t i);
    void set_indicated_speed(size_t i, float v);
    float get_speed_fraction(size_t i);
    float get_load_fraction(size_t i);
    bool get_missing(size_t i);
    void set_missing(size_t i, bool v);
    bool get_overload(size_t i);
    void set_overload(size_t i, bool v);
} // namespace pumps
