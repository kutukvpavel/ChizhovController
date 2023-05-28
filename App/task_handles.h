#pragma once

#include "FreeRTOS.h"
#include "task.h"

#define MY_ADC
#define MY_ENC
#define MY_IO
#define MY_THERMO

#define DECLARE_STATIC_TASK(name) extern TaskHandle_t task_handle_##name
#define STATIC_TASK_BODY(name) void start_task_##name(void *argument)

_BEGIN_STD_C

DECLARE_STATIC_TASK(MY_ADC);
DECLARE_STATIC_TASK(MY_ENC);
DECLARE_STATIC_TASK(MY_IO);
DECLARE_STATIC_TASK(MY_THERMO);

_END_STD_C
