#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct
{

} modbus_cmd_t;

typedef struct
{

} dbg_interop_t;

typedef enum 
{
    SET_SPEED,
    SET_DIRECTION,
    SET_ENABLE
} motor_cmds;
typedef struct
{
    size_t index;
    motor_cmds cmd;
    
} motor_cmd_t;