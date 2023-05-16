#include "sr_io.h"

#include "../Core/Inc/gpio.h"
#include "compat_api.h"

static inline void gpio_pulse_pin(GPIO_TypeDef* port, uint32_t mask, uint32_t delay = 1)
{
    LL_GPIO_SetOutputPin(IO_ST_GPIO_Port, IO_ST_Pin);
    compat::uDelay(delay);
    LL_GPIO_ResetOutputPin(IO_ST_GPIO_Port, IO_ST_Pin);
}

namespace sr_io
{
    SemaphoreHandle_t srMutexHandle = NULL;
    StaticSemaphore_t srMutexBuffer;

    void init()
    {
        srMutexHandle = xSemaphoreCreateMutexStatic(&srMutexBuffer);
        assert_param(srMutexHandle);


    }

    HAL_StatusTypeDef sync()
    {
        if (xSemaphoreTake(srMutexHandle, 0) != pdTRUE) return HAL_ERROR;

        //Write output register

        //Latch both registers
        gpio_pulse_pin(IO_ST_GPIO_Port, IO_ST_Pin);
        //Read input register

        xSemaphoreGive(srMutexHandle);
        return HAL_OK;
    }   
    bool get_input(size_t i)
    {

    } 
    void set_output(size_t i, bool v)
    {

    }
} // namespace sr_io
