#pragma once

#include "user.h"
#include "cmsis_os2.h"

#define MY_MB_USB 0
//#define MY_MB_DECLARE_REGS(name) extern uint16_t * const p_##name
#define GET_MB_REGS_PTR(name) mb_regs::p_##name
#define MY_MB_TIMEOUT 300 //ms?

namespace mb_regs
{
    /*extern const size_t length;
    MY_MB_DECLARE_REGS(MY_MB_USB);*/

    HAL_StatusTypeDef set_remote(bool enable);
} // namespace mb_regs