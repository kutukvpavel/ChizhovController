#pragma once

#include "user.h"
#include "motor.h"
#include "pumps.h"

namespace nvs
{
    HAL_StatusTypeDef init();
    HAL_StatusTypeDef save();
    HAL_StatusTypeDef load();
    HAL_StatusTypeDef reset();
    void dump_hex();
    HAL_StatusTypeDef test();

    uint8_t get_stored_version();
    uint8_t get_required_version();
    bool get_version_match();
    motor_reg_t* get_motor_regs();
    motor_params_t* get_motor_params();
    pumps::params_t* get_pump_params();
    uint16_t* get_modbus_addr();
    sr_buf_t* get_input_inversion();
} // namespace nvs
