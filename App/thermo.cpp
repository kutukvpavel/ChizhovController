#include "thermo.h"

#include "task_handles.h"
#include "wdt.h"
#include "spi_sync.h"

#include <math.h>

namespace thermo
{
    struct module_t
    {
        size_t spi_index;
        bool present;
    };
    static module_t modules[MY_TEMP_CHANNEL_NUM] = {
        {
            .spi_index = 2,
            .present = false
        },
        {
            .spi_index = 3,
            .present = false
        }
    };
    static float temperatures[MY_TEMP_CHANNEL_NUM] = { NAN, NAN };

    static float get_celsius(const uint8_t* buffer)
    {
        const float max_temp = 1024.0f;
        const float resolution = 4096.0f;

        uint16_t v = ((buffer[1] << 8) | buffer[0]) >> 3;
        return v * (max_temp / resolution);
    }

    static void init()
    {
        DBG("MAX6675 Init...");

        size_t i = 3;
        while (spi::acquire_bus(0) != HAL_OK)
        {
            vTaskDelay(pdMS_TO_TICKS(30));
            if (--i == 0) break;
        }
        if (i == 0)
        {
            ERR("Failed to acquire SPI bus for MAX6675 init!");
            return;
        }

        uint8_t buffer[2] = { 0xFF, 0xFF };
        for (i = 0; i < MY_TEMP_CHANNEL_NUM; i++)
        {
            HAL_StatusTypeDef res = spi::change_device(modules[i].spi_index);
            if (res != HAL_OK) continue;
            res = spi::receive(buffer, array_size(buffer)); //Given word length of 8 bits!
            modules[i].present = (res == HAL_OK) && (buffer[0] != 0xFF) && (buffer[1] != 0xFF); //Assuming MISO has a weak pullup
        }

        spi::release_bus();

        for (i = 0; i < MY_TEMP_CHANNEL_NUM; i++)
        {
            if (!modules[i].present) continue;
            DBG("Found MAX6675 #%u at CS_MUX=%u", i, modules[i].spi_index);
        }
    }

    static void sync()
    {
        uint8_t buffer[2];
        for (size_t i = 0; i < MY_TEMP_CHANNEL_NUM; i++)
        {
            if (!modules[i].present) continue;
            if (spi::acquire_bus(modules[i].spi_index) != HAL_OK) continue;
            HAL_StatusTypeDef res = spi::receive(buffer, array_size(buffer)); //Given word length of 8 bits!
            spi::release_bus();
            if (res != HAL_OK) continue;
            temperatures[i] = get_celsius(buffer); //float r/w on ARM is atomic, no need for a mutex
        }
    }

    const float* get_temperatures()
    {
        return temperatures;
    }
} // namespace thermo

_BEGIN_STD_C
STATIC_TASK_BODY(MY_THERMO)
{
    const uint32_t delay = 50;
    static TickType_t last_wake;
    static wdt::task_t* pwdt;

    pwdt = wdt::register_task(500, "max");

    thermo::init();
    INIT_NOTIFY(MY_THERMO);

    last_wake = xTaskGetTickCount();
    for (;;)
    {
        pwdt->last_time = xTaskGetTickCount();
        thermo::sync();
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(delay));
    }
}
_END_STD_C