#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define PACKED_FOR_MODBUS __packed __aligned(sizeof(float))

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

#ifdef __cplusplus
    template <typename T, size_t N>
    constexpr size_t array_size(T (&)[N]) { return N; }
#endif