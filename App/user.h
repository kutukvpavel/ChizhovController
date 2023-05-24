#pragma once

#include "../Middlewares/Third_Party/FreeRTOS/Source/include/FreeRTOS.h"
#include "../Middlewares/Third_Party/FreeRTOS/Source/include/task.h"
#include "../Middlewares/Third_Party/FreeRTOS/Source/include/queue.h"
#include "../Middlewares/Third_Party/FreeRTOS/Source/include/semphr.h"

#include "my_types.h"
#include "main.h"
#include "ushell/inc/sys_command_line.h"

#include <stdio.h>

/*#ifdef __cplusplus
#if !(defined(_BEGIN_STD_C) && defined(_END_STD_C))
#ifdef _HAVE_STD_CXX
#define _BEGIN_STD_C namespace std { extern "C" {
    
#define _END_STD_C  } }
#else
#define _BEGIN_STD_C extern "C" {
    
#define _END_STD_C  }
#endif
#endif
#else
#define _BEGIN_STD_C
#define _END_STD_C
#endif*/

#define MY_FIRMWARE_INFO_STR "reactor_ctrl-v0.1-a"

_BEGIN_STD_C

extern void StartAdcTask(void *argument);
extern void StartThermocoupleTask(void *argument);
extern void StartDisplayTask(void *argument);
extern void StartEncoderTask(void *argument);
extern void StartIoTask(void *argument);
extern void StartDebugMenuTask(void *argument);
extern void StartUsbTask(void *argument);
extern void StartRs485Task(void *argument);
extern void StartEthernetTask(void *argument);

_END_STD_C