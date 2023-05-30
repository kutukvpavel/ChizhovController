#include "i2c_sync.h"

#include "i2c.h"

#define ACQUIRE_MUTEX() if (xSemaphoreTake(mutex_handle, pdMS_TO_TICKS(timeout)) != pdTRUE) return HAL_BUSY
#define RELEASE_MUTEX() xSemaphoreGive(mutex_handle)

namespace i2c
{
    static SemaphoreHandle_t mutex_handle = NULL;
    static StaticSemaphore_t srMutexBuffer;

    HAL_StatusTypeDef init()
    {
        if (mutex_handle) return HAL_ERROR;
        
        mutex_handle = xSemaphoreCreateMutexStatic(&srMutexBuffer);
        return mutex_handle ? HAL_OK : HAL_ERROR;
    }

    HAL_StatusTypeDef read(uint16_t dev_addr, uint8_t* buffer, uint16_t len)
    {
        const uint32_t timeout = 20; //ms
        assert_param(mutex_handle);

        ACQUIRE_MUTEX();
        HAL_StatusTypeDef ret = HAL_I2C_Master_Receive(&hi2c2, dev_addr, buffer, len, timeout);

        RELEASE_MUTEX();
        return ret;
    }
    HAL_StatusTypeDef mem_read(uint16_t dev_addr, uint8_t mem_addr, uint8_t* buffer, uint16_t len)
    {
        const uint32_t timeout = 100; //ms
        assert_param(mutex_handle);

        ACQUIRE_MUTEX();
        HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(&hi2c2, dev_addr, mem_addr, I2C_MEMADD_SIZE_8BIT, buffer, len, timeout);

        RELEASE_MUTEX();
        return ret;
    }
    HAL_StatusTypeDef mem_write(uint16_t dev_addr, uint8_t mem_addr, uint8_t* buffer, uint16_t len)
    {
        const uint32_t timeout = 500; //ms
        assert_param(mutex_handle);

        ACQUIRE_MUTEX();
        HAL_StatusTypeDef ret = HAL_I2C_Mem_Write(&hi2c2, dev_addr, mem_addr, I2C_MEMADD_SIZE_8BIT, buffer, len, 500);

        RELEASE_MUTEX();
        return ret;
    }
} // namespace i2c
