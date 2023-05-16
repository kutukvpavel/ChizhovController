#pragma once

#include "user.h"

namespace sr_io
{
    enum 

    void init();

    HAL_StatusTypeDef sync();
    bool get_input(size_t i);
    void set_output(size_t i, bool v);
} // namespace sr_io
