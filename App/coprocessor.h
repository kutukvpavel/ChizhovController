#pragma once

#include "user.h"

namespace coprocessor
{
    uint32_t get_encoder_value(size_t i);
    float get_manual_override();
    bool get_encoder_btn_pressed(size_t i);
} // namespace coprocessor
