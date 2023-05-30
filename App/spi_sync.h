#include "user.h"

namespace spi
{
    HAL_StatusTypeDef init();

    HAL_StatusTypeDef acquire_bus(size_t device_index);
    HAL_StatusTypeDef receive(uint8_t* buffer, uint16_t len);
    HAL_StatusTypeDef release_bus();
} // namespace spi
