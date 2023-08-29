#pragma once

#include "FreeRTOS.h"
#include "task.h"

#define MY_ADC 0
#define MY_COPROC 1
#define MY_IO 2
#define MY_THERMO 3
#define MY_DISP 4
#define MY_CLI 5
#define MY_MODBUS 6
#define MY_FP 7
#define MY_ETH 8
//#define MY_PUMP 9
#define MY_WDT 9

#define STATIC_TASK_HANDLE(name) task_handle_##name
#define DECLARE_STATIC_TASK(name) extern TaskHandle_t task_handle_##name ; \
    extern void start_task_##name (void *argument)
#define STATIC_TASK_BODY(name) void start_task_##name (void *argument)
#define INIT_NOTIFY(name)   DBG("Task " #name " init OK!"); \
    assert_param(argument); \
    xTaskNotifyGive(*reinterpret_cast<TaskHandle_t*>(argument))

_BEGIN_STD_C

DECLARE_STATIC_TASK(MY_ADC);
DECLARE_STATIC_TASK(MY_COPROC);
DECLARE_STATIC_TASK(MY_IO);
DECLARE_STATIC_TASK(MY_THERMO);
DECLARE_STATIC_TASK(MY_DISP);
DECLARE_STATIC_TASK(MY_CLI);
DECLARE_STATIC_TASK(MY_MODBUS);
DECLARE_STATIC_TASK(MY_FP);
DECLARE_STATIC_TASK(MY_ETH);
//DECLARE_STATIC_TASK(MY_PUMP);
DECLARE_STATIC_TASK(MY_WDT);

_END_STD_C
