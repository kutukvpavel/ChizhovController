#include "spi_sync.h"

#include "sr_io.h"
#include "../Core/Inc/spi.h"

#define BV(i) (1u << (i))
#define B2PS(i, j) ((((i) & BV(j)) > 0) ? GPIO_PIN_SET : GPIO_PIN_RESET)

namespace spi
{
    static SemaphoreHandle_t mutex_handle = NULL;
    static StaticSemaphore_t srMutexBuffer;

    void set_cs_mux(size_t idx)
    {
        assert_param(idx < BV(3));

        HAL_GPIO_WritePin(CS_MUX_A_GPIO_Port, CS_MUX_A_Pin, B2PS(idx, 0));
        HAL_GPIO_WritePin(CS_MUX_B_GPIO_Port, CS_MUX_B_Pin, B2PS(idx, 1));
        HAL_GPIO_WritePin(CS_MUX_C_GPIO_Port, CS_MUX_C_Pin, B2PS(idx, 2));
    }

    HAL_StatusTypeDef init()
    {
        static uint8_t zero_arr[1] = {0};

        if (mutex_handle) return HAL_ERROR;

        set_cs_mux(0);
        HAL_SPI_Transmit(&hspi1, zero_arr, 1, 100);
        mutex_handle = xSemaphoreCreateRecursiveMutexStatic(&srMutexBuffer);
        return mutex_handle ? HAL_OK : HAL_ERROR;
    }

    HAL_StatusTypeDef acquire_bus(size_t device_index)
    {
        assert_param(device_index > 0);
        assert_param(mutex_handle);
        
        if (xSemaphoreTake(mutex_handle, pdMS_TO_TICKS(10)) != pdTRUE) return HAL_BUSY;

        set_cs_mux(device_index);

        //Do not release the mutex while the bus is claimed!
        return HAL_OK;
    }

    HAL_StatusTypeDef receive(uint8_t* buffer, uint16_t len)
    {
        assert_param(mutex_handle);
        if (xSemaphoreTakeRecursive(mutex_handle, 0) != pdTRUE) return HAL_ERROR;

        HAL_StatusTypeDef ret = HAL_SPI_Receive(&hspi1, buffer, len, 10);

        xSemaphoreGiveRecursive(mutex_handle);
        return ret;
    }

    HAL_StatusTypeDef release_bus()
    {
        assert_param(mutex_handle);

        set_cs_mux(0);

        //Release reviously acquired mutex!
        return (xSemaphoreGive(mutex_handle) == pdTRUE) ? HAL_OK : HAL_ERROR;
    }
} // namespace spi
