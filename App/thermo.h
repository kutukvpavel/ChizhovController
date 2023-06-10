#pragma once

#include "user.h"

#define MY_TEMP_CHANNEL_NUM 2

namespace thermo
{
    const float* get_temperatures();
    uint32_t get_recv_err_rate();

    const uint16_t* get_thermocouples_present();
    bool get_thermocouple_present(size_t i);
} // namespace thermo
