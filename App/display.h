#pragma once

#include <inttypes.h>
#include <stddef.h>

namespace display
{
    enum test_modes
    {
        none,
        all_lit,
        digits,
        all_high,
        all_low,

        TST_LEN
    };

    void set_lamp_test_mode(test_modes m);
    void set_edit(size_t channel, bool v);
} // namespace display
