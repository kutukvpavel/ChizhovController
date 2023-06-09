#pragma once

#include "user.h"

typedef uint16_t sr_buf_t;

#define BUF_WORD_BITS (__CHAR_BIT__ * sizeof(sr_buf_t))
#define BUF_LEN(n) (((n) - 1) / BUF_WORD_BITS + 1)

namespace sr_io
{
    const uint32_t regular_sync_delay_ms = 30;
    const uint32_t missed_sync_delay_ms = 3;

    enum in : size_t
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
    enum out : size_t
    {
        OUT_OPTO6, //User
        OUT_OPTO5, //User
        OUT_OPTO4, //User
        OUT_OPTO3, //User
        OUT_OPTO2, //User
        OUT_OPTO1, //User
        OUT_OPTO0, //User
        MOTOR_DIR_3,
        MOTOR_DIR_2,
        MOTOR_DIR_1,
        MOTOR_DIR_0,
        MOTORS_EN,
        OC12, //User
        OC11, //User
        OC10, //User
        OC9, //User
        OC8, //PCB error, user available
        OC6, //HW interlock enable
        OC5, //HW interlock lock pulse
        OC4,
        OC3,
        OC2,
        OC1,
        OC0,

        OUT_LEN
    };

    const size_t input_buffer_len = BUF_LEN(in::IN_LEN);
    const size_t output_buffer_len = BUF_LEN(out::OUT_LEN);
    
    bool get_input(in i);
    void set_output(out i, bool v);
    HAL_StatusTypeDef set_remote_commanded_outputs(sr_buf_t* p);
    const sr_buf_t* get_inputs();
    const sr_buf_t* get_outputs();
    bool get_output(out i);
    
    HAL_StatusTypeDef write_display(const void* data, size_t len, uint32_t wait);
} // namespace sr_io
