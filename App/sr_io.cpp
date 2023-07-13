#include "sr_io.h"

#include "../Core/Inc/gpio.h"
#include "compat_api.h"
#include "task_handles.h"
#include "wdt.h"
#include "nvs.h"

#define BIT_TO_WORD_IDX(b) ((b) / BUF_WORD_BITS)
#define BIT_REMAINDER_IDX(b) ((b) % BUF_WORD_BITS) 
#define BV(b) (1u << (b))
#define ACQUIRE_MUTEX() if (xSemaphoreTake(srMutexHandle, pdMS_TO_TICKS(wait)) != pdTRUE) return HAL_ERROR
#define RELEASE_MUTEX() xSemaphoreGive(srMutexHandle)

inline void gpio_positive_pulse_pin(GPIO_TypeDef *port, uint32_t mask, uint32_t delay = 1)
{
    LL_GPIO_SetOutputPin(port, mask);
    compat::uDelay(delay);
    LL_GPIO_ResetOutputPin(port, mask);
}
inline void gpio_negative_pulse_pin(GPIO_TypeDef *port, uint32_t mask, uint32_t delay = 1)
{
    LL_GPIO_ResetOutputPin(port, mask);
    compat::uDelay(delay);
    LL_GPIO_SetOutputPin(port, mask);
}

namespace sr_io
{
    static SemaphoreHandle_t srMutexHandle = NULL;
    static StaticSemaphore_t srMutexBuffer;
    static bool initialized = false;

    static sr_buf_t input_buffer[input_buffer_len];
    static sr_buf_t output_buffer[output_buffer_len];

    void init()
    {
        DBG("SR IO Init...");

        srMutexHandle = xSemaphoreCreateMutexStatic(&srMutexBuffer);
        assert_param(srMutexHandle);

        LL_GPIO_ResetOutputPin(SR_D_GPIO_Port, SR_D_Pin);
        for (size_t i = 0; i < out::OUT_LEN; i++)
        {
            gpio_positive_pulse_pin(OUT_SH_GPIO_Port, OUT_SH_Pin); //Clear the registers
        }
        LL_GPIO_SetOutputPin(IO_ST_GPIO_Port, IO_ST_Pin);

        initialized = true;
    }

    HAL_StatusTypeDef sync_io(uint32_t wait = missed_sync_delay_ms)
    {
        if (!initialized) return HAL_ERROR;
        ACQUIRE_MUTEX();

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
        //Handle input inversion
        sr_buf_t* inv = nvs::get_input_inversion();
        for (size_t i = 0; i < input_buffer_len; i++)
        {
            input_buffer[i] ^= inv[i];
        }
        
        RELEASE_MUTEX();
        return HAL_OK;
    }

    bool get_input(in i)
    {
        assert_param(i < in::IN_LEN);
        return input_buffer[BIT_TO_WORD_IDX(i)] & BV(BIT_REMAINDER_IDX(i));
    }
    const sr_buf_t* get_inputs()
    {
        return input_buffer;
    }
    void set_output(out i, bool v)
    {
        assert_param(i < out::OUT_LEN);

        if (v)
        {
            if (initialized) while (xSemaphoreTake(srMutexHandle, portMAX_DELAY) != pdTRUE);
            output_buffer[BIT_TO_WORD_IDX(i)] |= BV(BIT_REMAINDER_IDX(i));
        }
        else
        {
            if (initialized) while (xSemaphoreTake(srMutexHandle, portMAX_DELAY) != pdTRUE);
            output_buffer[BIT_TO_WORD_IDX(i)] &= ~BV(BIT_REMAINDER_IDX(i));
        }
        if (initialized) RELEASE_MUTEX();
    }
    HAL_StatusTypeDef set_remote_commanded_outputs(sr_buf_t* p)
    {
        if (!initialized) return HAL_ERROR;
        uint32_t wait = 20;

        sr_buf_t* mask = nvs::get_remote_output_mask();
        for (size_t i = 0; i < output_buffer_len; i++)
        {
            p[i] &= mask[i];
        }

        ACQUIRE_MUTEX();

        for (size_t i = 0; i < output_buffer_len; i++)
        {
            output_buffer[i] &= ~mask[i];
            output_buffer[i] |= p[i];
        }

        RELEASE_MUTEX();

        return HAL_OK;
    }
    const sr_buf_t* get_outputs()
    {
        return output_buffer;
    }
    bool get_output(out i)
    {
        assert_param(i < out::OUT_LEN);
        return output_buffer[BIT_TO_WORD_IDX(i)] & BV(BIT_REMAINDER_IDX(i));
    }

    HAL_StatusTypeDef write_display(const void* data, size_t len, uint32_t wait)
    {
        if (!initialized) return HAL_ERROR;
        ACQUIRE_MUTEX();

        for (size_t i = 0; i < len; i++)
        {
            for (size_t j = 0; j < __CHAR_BIT__; j++)
            {
                HAL_GPIO_WritePin(SR_D_GPIO_Port, SR_D_Pin,
                    (reinterpret_cast<const uint8_t*>(data)[len - i - 1] & BV(__CHAR_BIT__ - j - 1)) ? 
                        GPIO_PIN_SET : GPIO_PIN_RESET);
                compat::uDelay(2);
                gpio_positive_pulse_pin(DISP_SH_GPIO_Port, DISP_SH_Pin, 2);
            }
        }
        gpio_positive_pulse_pin(DISP_ST_GPIO_Port, DISP_ST_Pin, 2);

        RELEASE_MUTEX();
        return HAL_OK;
    }
} // namespace sr_io


_BEGIN_STD_C
STATIC_TASK_BODY(MY_IO)
{
    static TickType_t last_sync;
    static uint32_t delay = sr_io::regular_sync_delay_ms;
    static wdt::task_t* pwdt;

    sr_io::init();
    pwdt = wdt::register_task(500, "io");
    INIT_NOTIFY(MY_IO);
    last_sync = xTaskGetTickCount();

    for (;;)
    {
        vTaskDelayUntil(&last_sync, pdMS_TO_TICKS(delay));
        last_sync = xTaskGetTickCount();
        pwdt->last_time = last_sync;
        
        delay = (sr_io::sync_io() == HAL_OK) ? sr_io::regular_sync_delay_ms : sr_io::missed_sync_delay_ms;
    }
}
_END_STD_C