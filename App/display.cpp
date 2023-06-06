#include "display.h"

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
        paused = 13,
        running,
        overload
    };
    struct __packed single_channel_map
    {
        uint8_t digits[DIGITS_PER_CHANNEL];
        uint8_t leds[2];
    };
    static single_channel_map buffer[DISPLAY_CHANNELS] = { {}, {}, {} };
    static test_modes mode = test_modes::none;

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
    void convert_to_7seg(void* dest, const char* src)
    {
        const uint8_t digits[] = {
            0b11111100,
            0b01100000,
            0b11011010,
            0b11110010,
            0b01100110,
            0b10110110,
            0b10111110,
            0b11100000,
            0b11111110,
            0b11100110
        };
        const uint8_t point = 0x01;
        static_assert(array_size(digits) >= ('9' - '0'));

        auto d = reinterpret_cast<uint8_t*>(dest);
        size_t shift = 0;
        for (size_t i = 0; i < DIGITS_PER_CHANNEL; i++)
        {
            if (src[i + shift] == '.')
            {
                shift++;
                d[i - 1] |= point;
            }
            d[i] = ~digits[src[i + shift] - '0'];
        }
    }
    uint16_t convert_to_leds(float speed_fraction, float load_fraction)
    {
        return
            get_fraction_bits(speed_fraction, 8) << leds::speed_fraction |
            get_fraction_bits(load_fraction, 4) << leds::load_fraction;
    }

    void init()
    {
        DBG("Display init...");
    }

    HAL_StatusTypeDef repaint()
    {
        return sr_io::write_display(buffer, sizeof(buffer), MISSED_REPAINT_DELAY_MS);
    }

    void compose()
    {
        static char temp[DIGITS_PER_CHANNEL + 2]; //+ decimal point and a null character
        static_assert(DISPLAY_CHANNELS <= MY_PUMPS_NUM);

        test_modes m = mode;
        for (size_t i = 0; i < DISPLAY_CHANNELS; i++)
        {
            auto& b = buffer[i];
            switch (m)
            {
            case test_modes::all_lit:
                memset(&(b.digits), 0, sizeof(b.digits));
                memset(&(b.leds), 1, sizeof(b.leds));
                break;
            case test_modes::digits:
            {
                convert_to_7seg(&(b.digits), "123.4");
                uint16_t bits = convert_to_leds(0.45, 0.45);
                bits |= (1u << leds::overload) | (1u << leds::paused);
                b.leds[0] = (bits >> 8) & 0xFF;
                b.leds[1] = bits & 0xFF;
                break;
            }
            default:
            {
                if (pumps::get_missing(i))
                {
                    memset(&(b.digits), 1, sizeof(b.digits)); // Blank out missing channels
                    memset(&(b.leds), 0, sizeof(b.leds));
                    continue;
                }
                snprintf(temp, DIGITS_PER_CHANNEL + 1, "%5f", pumps::get_indicated_speed(i));
                convert_to_7seg(&(b.digits), temp);
                uint16_t bits = convert_to_leds(pumps::get_speed_fraction(i), pumps::get_load_fraction(i));
                if (pumps::get_overload(i)) bits |= (1u << leds::overload);
                if (pumps::get_paused(i)) bits |= (1u << leds::paused);
                b.leds[0] = (bits >> 8) & 0xFF;
                b.leds[1] = bits & 0xFF;
                break;
            }
            }
        }
    }

    void set_lamp_test_mode(test_modes m)
    {
        mode = m;
    }
} // namespace display

_BEGIN_STD_C
STATIC_TASK_BODY(MY_DISP)
{
    static TickType_t last_repaint;
    static uint32_t delay = REGULAR_REPAINT_DELAY_MS;

    display::init();
    INIT_NOTIFY(MY_DISP);
    last_repaint = xTaskGetTickCount();

    for (;;)
    {
        vTaskDelayUntil(&last_repaint, pdMS_TO_TICKS(delay));
        display::compose();
        delay = (display::repaint() == HAL_OK) ? REGULAR_REPAINT_DELAY_MS : MISSED_REPAINT_DELAY_MS;
    }
}
_END_STD_C