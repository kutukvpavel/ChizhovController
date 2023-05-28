#pragma once

#include "FreeRTOS.h"
#include "task.h"

#define MY_ADC
#define MY_ENC
#define MY_IO
#define MY_THERMO
#define MY_DISP
#define MY_CLI

#define STATIC_TASK_HANDLE(name) task_handle_##name
#define DECLARE_STATIC_TASK(name) extern TaskHandle_t task_handle_##name ; \
    extern void start_task_##name (void *argument)
#define STATIC_TASK_BODY(name) void start_task_##name (void *argument)

_BEGIN_STD_C

DECLARE_STATIC_TASK(MY_ADC);
DECLARE_STATIC_TASK(MY_ENC);
DECLARE_STATIC_TASK(MY_IO);
DECLARE_STATIC_TASK(MY_THERMO);
DECLARE_STATIC_TASK(MY_DISP);
DECLARE_STATIC_TASK(MY_CLI);

_END_STD_C
