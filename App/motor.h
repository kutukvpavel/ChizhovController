#pragma once

#include "user.h"
#include "tim.h"
#include "sr_io.h"

struct PACKED_FOR_MODBUS motor_params_t
{
    uint16_t dir;
    uint16_t microsteps;
    uint16_t teeth;
    uint16_t reserved1; //alignment
    float volume_rate_to_rps;
    float max_rate_rps;
    float max_load_err;
};
struct PACKED_FOR_MODBUS motor_reg_t
{
    float volume_rate;
    float rps;
    float err;
    uint16_t status;
    uint16_t reserved1; //Alignment
    float time_left;
};

class motor_t
{
private:
    TIM_HandleTypeDef* timer;
    uint32_t timer_channel;
    sr_io::out pin_dir;
    float last_volume_rate = 0;
    float current_load_err;
    size_t current_range = 0;
    motor_params_t params;
    motor_reg_t* reg;
    SemaphoreHandle_t params_mutex = NULL;
public:
    enum status_bits : uint16_t
    {
        missing = 0,
        overload,
        paused,
        running,
        on_timer,
        timer_completed
    };

    motor_t(TIM_HandleTypeDef* tim, uint32_t channel, sr_io::out dir, const motor_params_t* p, motor_reg_t* r);
    ~motor_t();

    HAL_StatusTypeDef reload_params();
    void print_debug_info();

    float get_volume_rate_limit();
    HAL_StatusTypeDef set_volume_rate(float v);
    float get_volume_rate();
    float get_speed_fraction();
    void set_load_err(float v);
    float get_load_fraction();
    bool get_overload();
    void set_missing(bool v);
    bool get_missing();
    void set_paused(bool v);
    bool get_paused();
    void set_disabled_by_timer(bool v);
    
    bool check_status_bit(status_bits b);
    void set_status_bit(status_bits b, bool v);
};
