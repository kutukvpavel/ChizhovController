#include "thermo.h"

#include "task_handles.h"
#include "wdt.h"
#include "spi_sync.h"
#include "average/average.h"

#include <math.h>

#define AVERAGING 4 //pts

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
    static uint16_t thermocouple_present_bitfield = 0;
    static uint32_t receive_error_rate = 0;
    average containers[MY_TEMP_CHANNEL_NUM] = { average(AVERAGING), average(AVERAGING) };

    static float get_celsius(const uint8_t* buffer)
    {
        const float max_temp = 1024.0f;
        const float resolution = 4096.0f;

        uint16_t v = ((buffer[0] << 8) | buffer[1]) >> 3;
        return v * (max_temp / resolution);
    }

    static void init()
    {
        static_assert((sizeof(thermocouple_present_bitfield) * __CHAR_BIT__) >= MY_TEMP_CHANNEL_NUM);
        DBG("MAX6675 Init...");

        size_t i = 3;
        while (--i)
        {
            if (spi::acquire_bus(0) == HAL_OK) break;
            vTaskDelay(pdMS_TO_TICKS(30));
        }
        if (i == 0)
        {
            ERR("Failed to acquire SPI bus for init!");
            return;
        }
        else
        {
            DBG("Acquired SPI bus for module probing.");
        }

        uint8_t buffer[2] = { 0xFF, 0xFF };
        for (i = 0; i < MY_TEMP_CHANNEL_NUM; i++)
        {
            DBG("Setting CS_MUX to %u", modules[i].spi_index);
            HAL_StatusTypeDef res = spi::change_device(modules[i].spi_index);
            if (res != HAL_OK) continue;
            DBG("Probing #%u", i);
            res = spi::receive(buffer, array_size(buffer)); //Given word length of 8 bits!
            modules[i].present = (res == HAL_OK) && (buffer[0] != 0xFF) && (buffer[1] != 0xFF); //Assuming MISO has a weak pullup
        }

        spi::release_bus();
        DBG("Released SPI bus");

        for (i = 0; i < MY_TEMP_CHANNEL_NUM; i++)
        {
            if (!modules[i].present) continue;
            DBG("Found module #%u at CS_MUX=%u", i, modules[i].spi_index);
        }
    }

    static void sync()
    {
        uint8_t buffer[2];
        for (size_t i = 0; i < MY_TEMP_CHANNEL_NUM; i++)
        {
            if (!modules[i].present) continue;
            if (spi::acquire_bus(modules[i].spi_index) != HAL_OK)
            {
                if (receive_error_rate < UINT32_MAX) receive_error_rate++;
                continue;
            }
            HAL_StatusTypeDef res = spi::receive(buffer, array_size(buffer)); //Given word length of 8 bits!
            spi::release_bus();
            if (res != HAL_OK) 
            {
                if (receive_error_rate < UINT32_MAX) receive_error_rate++;
                continue;
            }
            containers[i].enqueue(get_celsius(buffer));
            if (buffer[1] & (1u << 2)) 
            {
                thermocouple_present_bitfield &= ~(1u << i); //Remove present bit first
                temperatures[i] = NAN; //Set NAN second
            }
            else  
            {
                temperatures[i] = containers[i].get_average(); //float r/w on ARM is atomic, no need for a mutex
                thermocouple_present_bitfield |= (1u << i);
            }
        }
    }

    const float* get_temperatures()
    {
        return temperatures;
    }
    const uint16_t* get_thermocouples_present()
    {
        return &thermocouple_present_bitfield;
    }
    bool get_thermocouple_present(size_t i)
    {
        return (thermocouple_present_bitfield & (1u << i)) > 0;
    }
    uint32_t get_recv_err_rate()
    {
        return receive_error_rate;
    }
} // namespace thermo

_BEGIN_STD_C
STATIC_TASK_BODY(MY_THERMO)
{
    const uint32_t delay = 250;
    static TickType_t last_wake;
    static wdt::task_t* pwdt;

    thermo::init();
    pwdt = wdt::register_task(1000, "max");
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