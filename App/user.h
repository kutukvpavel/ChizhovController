#pragma once

#include "../Middlewares/Third_Party/FreeRTOS/Source/include/FreeRTOS.h"
#include "../Middlewares/Third_Party/FreeRTOS/Source/include/task.h"
#include "../Middlewares/Third_Party/FreeRTOS/Source/include/queue.h"
#include "../Middlewares/Third_Party/FreeRTOS/Source/include/semphr.h"

#include "my_types.h"
#include "main.h"

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