#include "user.h"

#include "sr_io.h"
#include "pumps.h"
#include "task_handles.h"

#include <string.h>

#define REGULAR_REPAINT_DELAY_MS 50
#define MISSED_REPAINT_DELAY_MS 3
#define DISPLAY_CHANNELS 3
#define DIGITS_PER_CHANNEL 4

namespace display
{
    enum leds : size_t
    {
        load_fraction = 0,
        speed_fraction = 4,
        overload = 14,
        running
    };
    struct __packed single_channel_map
    {
        uint8_t digits[DIGITS_PER_CHANNEL];
        uint8_t leds[2];
    };
    single_channel_map buffer[DISPLAY_CHANNELS] = { {}, {}, {} };

    uint8_t get_fraction_bits(float v, size_t total_bits)
    {
        assert_param(total_bits <= 8);

        uint8_t res = 0;
        float resolution = 1.0f / total_bits;
        for (size_t i = 0; i < total_bits; i++)
        {
            if (v < (i * resolution)) break;
            res |= (1u << i);
        }
        return res;
    }

    void init()
    {

    }

    HAL_StatusTypeDef repaint()
    {
        return sr_io::write_display(buffer, sizeof(buffer), MISSED_REPAINT_DELAY_MS);
    }

    void compose()
    {
        static char temp[DIGITS_PER_CHANNEL + 1];
        static_assert(DISPLAY_CHANNELS <= MY_PUMPS_NUM);

        for (size_t i = 0; i < DISPLAY_CHANNELS; i++)
        {
            if (pumps::get_missing(i))
            {
                memset(&(buffer[i]), 0, sizeof(buffer[i])); //Blank out missing channels
                continue;
            }
            snprintf(temp, DIGITS_PER_CHANNEL + 1, "%4f", pumps::get_indicated_speed(i));
            memcpy(buffer[i].digits, temp, DIGITS_PER_CHANNEL);
            uint16_t bits = 
                get_fraction_bits(pumps::get_speed_fraction(i), 8) << leds::speed_fraction | 
                get_fraction_bits(pumps::get_load_fraction(i), 4) << leds::load_fraction;
            if (pumps::get_overload(i)) bits |= (1u << leds::overload);
        }
    }
} // namespace display

_BEGIN_STD_C
STATIC_TASK_BODY(MY_DISP)
{
    static TickType_t last_repaint;
    static uint32_t delay = REGULAR_REPAINT_DELAY_MS;

    display::init();
    last_repaint = xTaskGetTickCount();

    for (;;)
    {
        vTaskDelayUntil(&last_repaint, pdMS_TO_TICKS(delay));
        display::compose();
        delay = (display::repaint() == HAL_OK) ? REGULAR_REPAINT_DELAY_MS : MISSED_REPAINT_DELAY_MS;
    }
}
_END_STD_C