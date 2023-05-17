#pragma once

#include "user.h"

namespace sr_io
{
    const uint32_t regular_sync_delay_ms = 100;
    const uint32_t missed_sync_delay_ms = 3;

    enum in
    {
        IN_OPTO3,
        IN_OPTO2,
        IN_OPTO1,
        IN_OPTO0,
        IN11,
        IN10,
        IN9,
        IN8,
        IN7,
        IN6,
        IN5,
        IN4,
        IN3,
        IN2,
        IN1,
        IN0,

        IN_LEN
    };
    enum out
    {
        OC0,
        OC1,
        OC2,
        OC3,
        OC4,
        OC5,
        OC6,
        OC8, //PCB error
        OC9,
        OC10,
        OC11,
        OC12,
        MOTORS_EN,
        MOTOR_DIR_0,
        MOTOR_DIR_1,
        MOTOR_DIR_2,
        MOTOR_DIR_3,
        OUT_OPTO0,
        OUT_OPTO1,
        OUT_OPTO2,
        OUT_OPTO3,
        OUT_OPTO4,
        OUT_OPTO5,
        OUT_OPTO6,

        OUT_LEN
    };
    
    bool get_input(in i);
    void set_output(out i, bool v);
    
    HAL_StatusTypeDef write_display(const void* data, size_t len, uint32_t wait);
} // namespace sr_io
