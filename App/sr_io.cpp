#include "sr_io.h"

#include "../Core/Inc/gpio.h"
#include "compat_api.h"

typedef uint32_t sr_buf_t;

#define BUF_WORD_BITS (__CHAR_BIT__ * sizeof(sr_buf_t))
#define BUF_LEN(n) (((n) - 1) / BUF_WORD_BITS + 1)
#define BIT_TO_WORD_IDX(b) ((b) / BUF_WORD_BITS)
#define BIT_REMAINDER_IDX(b) ((b) % BUF_WORD_BITS) 
#define BV(b) (1u << (b))

    inline void gpio_positive_pulse_pin(GPIO_TypeDef* port, uint32_t mask, uint32_t delay = 1)
    {
        LL_GPIO_SetOutputPin(port, mask);
        compat::uDelay(delay);
        LL_GPIO_ResetOutputPin(port, mask);
    }
    inline void gpio_negative_pulse_pin(GPIO_TypeDef* port, uint32_t mask, uint32_t delay = 1)
    {
        LL_GPIO_ResetOutputPin(port, mask);
        compat::uDelay(delay);
        LL_GPIO_SetOutputPin(port, mask);
    }

namespace sr_io
{
    static SemaphoreHandle_t srMutexHandle = NULL;
    static StaticSemaphore_t srMutexBuffer;

    static sr_buf_t input_buffer[BUF_LEN(in::IN_LEN)];
    static sr_buf_t output_buffer[BUF_LEN(out::OUT_LEN)];

    void init()
    {
        srMutexHandle = xSemaphoreCreateMutexStatic(&srMutexBuffer);
        assert_param(srMutexHandle);

        LL_GPIO_ResetOutputPin(SR_D_GPIO_Port, SR_D_Pin);
        for (size_t i = 0; i < out::OUT_LEN; i++)
        {
            gpio_positive_pulse_pin(OUT_SH_GPIO_Port, OUT_SH_Pin); //Clear the registers
        }
        LL_GPIO_SetOutputPin(IO_ST_GPIO_Port, IO_ST_Pin);
    }

    HAL_StatusTypeDef sync_io(uint32_t wait = missed_sync_delay_ms)
    {
        if (xSemaphoreTake(srMutexHandle, pdMS_TO_TICKS(wait)) != pdTRUE) return HAL_ERROR;

        //Write output register
        for (size_t i = 0; i < out::OUT_LEN; i++)
        {
            HAL_GPIO_WritePin(SR_D_GPIO_Port, SR_D_Pin, 
                (output_buffer[BIT_TO_WORD_IDX(i)] & BV(BIT_REMAINDER_IDX(i))) ? GPIO_PIN_SET : GPIO_PIN_RESET);
            compat::uDelay(1);
            gpio_positive_pulse_pin(OUT_SH_GPIO_Port, OUT_SH_Pin);
        }
        //Latch both registers
        gpio_negative_pulse_pin(IO_ST_GPIO_Port, IO_ST_Pin);
        //Read input register
        for (size_t i = 0; i < in::IN_LEN; i++)
        {
            if (HAL_GPIO_ReadPin(IN_D_GPIO_Port, IN_D_Pin) == GPIO_PIN_SET)
            {
                input_buffer[BIT_TO_WORD_IDX(i)] |= BV(BIT_REMAINDER_IDX(i));
            }
            else
            {
                input_buffer[BIT_TO_WORD_IDX(i)] &= ~BV(BIT_REMAINDER_IDX(i));
            }
            gpio_positive_pulse_pin(IN_SH_GPIO_Port, IN_SH_Pin);
        }

        xSemaphoreGive(srMutexHandle);
        return HAL_OK;
    }   
    bool get_input(in i)
    {
        return input_buffer[BIT_TO_WORD_IDX(i)] & BV(BIT_REMAINDER_IDX(i));
    } 
    void set_output(out i, bool v)
    {
        if (v)
        {
            output_buffer[BIT_TO_WORD_IDX(i)] |= BV(BIT_REMAINDER_IDX(i));
        }
        else
        {
            output_buffer[BIT_TO_WORD_IDX(i)] &= ~BV(BIT_REMAINDER_IDX(i));
        }
    }

    HAL_StatusTypeDef write_display(const void* data, size_t len, uint32_t wait)
    {
        if (xSemaphoreTake(srMutexHandle, pdMS_TO_TICKS(wait)) != pdTRUE) return HAL_ERROR;

        

        xSemaphoreGive(srMutexHandle);
        return HAL_OK;
    }
} // namespace sr_io


_BEGIN_STD_C

void StartIoTask(void *argument)
{
    static TickType_t last_sync;
    static uint32_t delay = sr_io::regular_sync_delay_ms;

    sr_io::init();
    last_sync = xTaskGetTickCount();

    for (;;)
    {
        vTaskDelayUntil(&last_sync, pdMS_TO_TICKS(delay));
        delay = (sr_io::sync_io() == HAL_OK) ? sr_io::regular_sync_delay_ms : sr_io::missed_sync_delay_ms;
    }
}

_END_STD_C