#include "display.h"

#include "user.h"
#include "sr_io.h"
#include "pumps.h"
#include "task_handles.h"
#include "wdt.h"
#include "interop.h"

#include <string.h>
#include <math.h>

#define REGULAR_REPAINT_DELAY_MS 50
#define MISSED_REPAINT_DELAY_MS 3
#define DISPLAY_CHANNELS 3
#define DIGITS_PER_CHANNEL 4

#ifndef BV
 #define BV(i) (1u << (i))
#endif
#define GENERATE_DIGIT_TABLE(prefix) { \
            BV(prefix ## _A) | BV(prefix ## _B) | BV(prefix ## _C) | BV(prefix ## _D) | BV(prefix ## _E) | BV(prefix ## _F), \
            BV(prefix ## _B) | BV(prefix ## _C), \
            BV(prefix ## _A) | BV(prefix ## _B) | BV(prefix ## _D) | BV(prefix ## _E) | BV(prefix ## _G), \
            BV(prefix ## _A) | BV(prefix ## _B) | BV(prefix ## _C) | BV(prefix ## _D) | BV(prefix ## _G), \
            BV(prefix ## _B) | BV(prefix ## _C) | BV(prefix ## _F) | BV(prefix ## _G), \
            BV(prefix ## _A) | BV(prefix ## _C) | BV(prefix ## _D) | BV(prefix ## _F) | BV(prefix ## _G), \
            BV(prefix ## _A) | BV(prefix ## _C) | BV(prefix ## _D) | BV(prefix ## _E) | BV(prefix ## _F) | BV(prefix ## _G), \
            BV(prefix ## _A) | BV(prefix ## _B) | BV(prefix ## _C), \
            BV(prefix ## _A) | BV(prefix ## _B) | BV(prefix ## _C) | BV(prefix ## _D) | BV(prefix ## _E) | BV(prefix ## _F) | BV(prefix ## _G), \
            BV(prefix ## _A) | BV(prefix ## _B) | BV(prefix ## _C) | BV(prefix ## _D) | BV(prefix ## _F) | BV(prefix ## _G) }
#define ODD
#define EVEN

namespace display
{
    enum leds : size_t
    {
        overload = 0,
        paused,
        running,
        speed_fraction = 4,
        load_fraction = 12
    };
    struct __packed single_channel_map
    {
        uint8_t digits[DIGITS_PER_CHANNEL];
        uint8_t leds[2];
    };
    static single_channel_map buffer[DISPLAY_CHANNELS] = { {}, {}, {} };
    static bool edit[DISPLAY_CHANNELS];
    static test_modes mode = test_modes::none;

    uint8_t get_fraction_bits(float v, size_t total_bits)
    {
        assert_param(total_bits <= 8);
        if (v <= __FLT_EPSILON__) return 0;

        size_t n = static_cast<size_t>(roundf(v * total_bits));
        if (n > total_bits) n = total_bits;
        return static_cast<uint8_t>(static_cast<uint16_t>(1u << n) - 1) | 0x01;
    }
    void convert_to_7seg(void* dest, const char* src)
    {
        enum odd_digit_bits
        {
            ODD_DP,
            ODD_C,
            ODD_D,
            ODD_E,
            ODD_F,
            ODD_G,
            ODD_A,
            ODD_B,

            ODD_LEN
        };
        enum even_digit_bits
        {
            EVEN_DP,
            EVEN_C,
            EVEN_G,
            EVEN_D,
            EVEN_E,
            EVEN_F,
            EVEN_A,
            EVEN_B,

            EVEN_LEN
        };
        static_assert(ODD_LEN == 8);
        static_assert(EVEN_LEN == 8);

        static const uint8_t odd_digits[] = GENERATE_DIGIT_TABLE(ODD);
        static const uint8_t even_digits[] = GENERATE_DIGIT_TABLE(EVEN);
        static_assert(array_size(odd_digits) >= ('9' - '0'));
        static_assert(array_size(even_digits) >= ('9' - '0'));

        auto d = reinterpret_cast<uint8_t*>(dest);
        size_t shift = 0;
        for (size_t i = 0; i < DIGITS_PER_CHANNEL; i++)
        {
            if (src[i + shift] == '.')
            {
                shift++;
                d[i - 1] &= ~BV((i % 2 == 0) ? static_cast<uint32_t>(ODD_DP) : static_cast<uint32_t>(EVEN_DP)); //Common-anode!
            }
            char c = src[i + shift];
            switch (c)
            {
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                    //Not a mistake, even/odd meant counting from 1
                    d[i] = ~((i % 2 == 0) ? odd_digits : even_digits)[c - '0'];
                    break;
                default:
                    d[i] = 0xFF; //empty digit
                    break;
            }
            
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
        static const TickType_t edit_blink_delay = pdMS_TO_TICKS(400);

        static TickType_t last_edit_blink_toggle[DISPLAY_CHANNELS] = {};
        static char temp[DIGITS_PER_CHANNEL + 3]; //+ decimal point and a null character
        static bool interop_running = false;
        static_assert(DISPLAY_CHANNELS <= MY_PUMPS_NUM);

        const uint8_t* p;
        test_modes m = mode; //double-buffer this var
        if (interop::try_receive(interop::cmds::lamp_test_predefined, reinterpret_cast<const void**>(&p)) == HAL_OK)
        {
            m = static_cast<display::test_modes>(*p);
            mode = m;
        }
        if (m == test_modes::none)
        {
            static uint8_t byte_pattern;
            bool have_new_interop = 
                (interop::try_receive(interop::cmds::lamp_test_custom, reinterpret_cast<const void**>(&p)) == HAL_OK);
            if (have_new_interop) 
            {
                assert_param(p);
                byte_pattern = *p;
                DBG("Lamp test interop initiated: pattern = 0x%hX", byte_pattern);
                interop_running = true;
            }
            if (interop_running)
            {
                for (size_t i = 0; i < DISPLAY_CHANNELS; i++)
                {
                    auto& b = buffer[i];
                    memset(&(b.digits), byte_pattern, sizeof(b.digits));
                    memset(&(b.leds), byte_pattern, sizeof(b.leds));
                }
                return;
            }
        }
        else
        {
            interop_running = false;
        }

        for (size_t i = 0; i < DISPLAY_CHANNELS; i++)
        {
            auto& b = buffer[i];
            switch (m)
            {
            case test_modes::all_lit:
                memset(&(b.digits), 0, sizeof(b.digits));
                memset(&(b.leds), 0xFF, sizeof(b.leds));
                break;
            case test_modes::digits:
            {
                convert_to_7seg(&(b.digits), "12.34");
                uint16_t bits = convert_to_leds(0.45, 0.45);
                bits |= (1u << leds::overload) | (1u << leds::paused);
                b.leds[0] = (bits >> 8) & 0xFF;
                b.leds[1] = bits & 0xFF;
                break;
            }
            case test_modes::all_high:
                memset(&(b.digits), 0xFF, sizeof(b.digits));
                memset(&(b.leds), 0xFF, sizeof(b.leds));
                break;
            case test_modes::all_low:
                memset(&(b.digits), 0, sizeof(b.digits));
                memset(&(b.leds), 0, sizeof(b.leds));
                break;
            default:
            {
                TickType_t now = xTaskGetTickCount();
                if (pumps::get_missing(i) && !edit[i])
                {
                    memset(&(b.digits), 0xFF, sizeof(b.digits)); // Blank out missing channels (or during edit blinking)
                    memset(&(b.leds), 0, sizeof(b.leds));
                    continue;
                }
                if (edit[i] && (now - last_edit_blink_toggle[i]) < edit_blink_delay)
                {
                    memset(&(b.digits), 0xFF, sizeof(b.digits));
                    continue;
                }
                if (edit[i])
                {
                    if ((now - last_edit_blink_toggle[i]) > (edit_blink_delay * 2))
                        last_edit_blink_toggle[i] = now;
                }
                snprintf(temp, DIGITS_PER_CHANNEL + 2, "%5f", pumps::get_indicated_speed(i));
                convert_to_7seg(&(b.digits), temp);
                uint16_t bits = convert_to_leds(pumps::get_speed_fraction(i),
                    pumps::get_running(i) ? pumps::get_load_fraction(i) : 0);
                if (pumps::get_overload(i)) bits |= (1u << leds::overload);
                if (pumps::get_paused(i)) bits |= (1u << leds::paused);
                if (pumps::get_running(i)) bits |= (1u << leds::running);
                b.leds[0] = bits & 0xFF;
                b.leds[1] = (bits >> 8) & 0xFF;
                break;
            }
            }
        }
    }

    void set_lamp_test_mode(test_modes m)
    {
        mode = m;
    }
    void set_edit(size_t channel, bool v)
    {
        static_assert(MY_PUMPS_NUM == DISPLAY_CHANNELS);
        assert_param(channel < MY_PUMPS_NUM);

        edit[channel] = v;
    }
} // namespace display

_BEGIN_STD_C
STATIC_TASK_BODY(MY_DISP)
{
    static TickType_t last_repaint;
    static uint32_t delay = REGULAR_REPAINT_DELAY_MS;
    static wdt::task_t* pwdt;

    display::init();
    pwdt = wdt::register_task(400, "disp");
    INIT_NOTIFY(MY_DISP);
    last_repaint = xTaskGetTickCount();

    for (;;)
    {
        vTaskDelayUntil(&last_repaint, pdMS_TO_TICKS(delay));
        display::compose();
        delay = (display::repaint() == HAL_OK) ? REGULAR_REPAINT_DELAY_MS : MISSED_REPAINT_DELAY_MS;
        pwdt->last_time = xTaskGetTickCount();
    }
}
_END_STD_C