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

    float get_input(in i);
} // namespace a_io
