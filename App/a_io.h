#pragma once

#include "user.h"

namespace a_io
{
    enum in
    {
        ambient_temp,
        analog_thermocouple,
        vref,
        vbat,

        LEN
    };
    struct cal_t
    {
        float k = 1;
        float b = 0;
    };

    float get_input(in i);
    float get_hot_junction_temp();
} // namespace a_io
