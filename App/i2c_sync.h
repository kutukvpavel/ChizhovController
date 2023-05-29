#pragma once

#include "user.h"
#include "i2c.h"

namespace i2c
{
    HAL_StatusTypeDef init();
    
    HAL_StatusTypeDef mem_read(uint16_t dev_addr, uint8_t mem_addr, uint8_t* buffer, uint16_t len);
    HAL_StatusTypeDef mem_write(uint16_t dev_addr, uint8_t mem_addr, uint8_t* buffer, uint16_t len);
} // namespace i2c
