#pragma once

#include "main.h"

#define MY_TIM_MICROS TIM5

namespace compat
{
    void uDelay(uint32_t us);

    inline uint32_t micros()
    {
        return MY_TIM_MICROS->CNT;
    }
} // namespace compat
