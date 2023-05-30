#include "thermo.h"

#include "task_handles.h"
#include "wdt.h"
#include "spi_sync.h"

#include <math.h>

namespace thermo
{
    size_t spi_indexes[MY_TEMP_CHANNEL_NUM] = { 2, 3 };
    float temperatures[MY_TEMP_CHANNEL_NUM] = { NAN, NAN };

    float get_celsius(const uint8_t* buffer)
    {
        const float max_temp = 1024.0f;
        const float resolution = 4096.0f;

        uint16_t v = ((buffer[1] << 8) | buffer[0]) >> 3;
        return v * (max_temp / resolution);
    }

    void init()
    {

    }

    void sync()
    {
        uint8_t buffer[2];
        for (size_t i = 0; i < MY_TEMP_CHANNEL_NUM; i++)
        {
            if (spi::acquire_bus(spi_indexes[i]) != HAL_OK) continue;
            HAL_StatusTypeDef res = spi::receive(buffer, array_size(buffer)); //Given word length of 8 bits!
            spi::release_bus();
            if (res != HAL_OK) continue;
            temperatures[i] = get_celsius(buffer); //float r/w on ARM is atomic, no need for a mutex
        }
    }

    float get_temperature(size_t i)
    {
        assert_param(i < array_size(temperatures));
        return temperatures[i];
    }
} // namespace thermo

_BEGIN_STD_C
STATIC_TASK_BODY(MY_THERMO)
{
    const uint32_t delay = 50;
    static TickType_t last_wake;
    static wdt::task_t* pwdt;

    pwdt = wdt::register_task(500);

    thermo::init();

    for (;;)
    {
        thermo::sync();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(delay));
        pwdt->last_time = last_wake;
    }
}
_END_STD_C