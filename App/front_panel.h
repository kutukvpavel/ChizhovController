#pragma once

#include "user.h"

namespace front_panel
{
    enum lights : size_t
    {
        l_start = 0,
        l_stop,
        l_automatic_mode,
        l_manual_override,
        l_emergency,

        L_LEN
    };
    enum buttons : size_t
    {
        b_emergency = 0,
        b_start,
        b_stop,
        b_pause,
        b_light_test,

        B_LEN
    };
    enum class l_state
    {
        off,
        on,
        blink
    };

    void clear_lights();
    void set_light(lights l, l_state on, size_t channel = 0);
    bool get_button(buttons b, size_t channel = 0);
} // namespace front_panel
