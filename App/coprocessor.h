#pragma once

#include "user.h"

namespace coprocessor
{
    bool get_initialized();
    HAL_StatusTypeDef reinit();
    uint32_t get_crc_err_stats();

    uint32_t get_encoder_value(size_t i);
    float get_manual_override();
    bool get_encoder_btn_pressed(size_t i);
    bool get_drv_error(size_t i);
    bool get_drv_missing(size_t i);
} // namespace coprocessor
