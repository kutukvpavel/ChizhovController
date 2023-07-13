#pragma once

#include "user.h"
#include "motor.h"

#define MY_PUMPS_NUM 3 //Currently implemented for the whole system
#define MY_PUMPS_MAX 4 //Max for main board

namespace pumps
{
    struct PACKED_FOR_MODBUS params_t
    {
        uint16_t invert_enable;
        uint16_t reserved; //Alignment
        float volume_rate_resolution;
    };

    void init(const params_t* p, const motor_params_t* mp, motor_reg_t* mr);

    void reload_motor_params();
    void reload_params();
    void update_manual_override();
    void print_debug_info();
    void update_load();

    void set_enable(bool v);
    bool get_enabled();
    void enable_hw_interlock(bool v);
    void switch_hw_interlock();
    bool get_hw_interlock_ok();

    bool get_running(size_t i);
    float get_indicated_speed(size_t i);
    HAL_StatusTypeDef set_indicated_speed(size_t i, float v);
    HAL_StatusTypeDef increment_speed(size_t i, int32_t diff);
    float get_speed_fraction(size_t i);
    float get_load_fraction(size_t i);
    bool get_missing(size_t i);
    void set_missing(size_t i, bool v);
    bool get_overload(size_t i);
    bool get_paused(size_t i);
    void set_paused(size_t i, bool v);
    bool get_consistent_overload();
    void reset_consistent_overload();
    void reload_motor_regs();
    void stop_all();
} // namespace pumps
