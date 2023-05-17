#include "user.h"
#include "sr_io.h"

#include <string.h>

#define REGULAR_REPAINT_DELAY_MS 50
#define MISSED_REPAINT_DELAY_MS 3
#define DISPLAY_CHANNELS 3
#define DIGITS_PER_CHANNEL 4

namespace display
{
    struct __packed single_channel_map
    {
        uint8_t digits[DIGITS_PER_CHANNEL];
        uint8_t leds[2];
    };
    single_channel_map buffer[DISPLAY_CHANNELS];

    void init()
    {

    }

    HAL_StatusTypeDef repaint()
    {
        return sr_io::write_display(&buffer, sizeof(buffer), MISSED_REPAINT_DELAY_MS);
    }

    void compose()
    {
        static char temp[DIGITS_PER_CHANNEL + 1];

        for (size_t i = 0; i < DISPLAY_CHANNELS; i++)
        {
            snprintf(temp, DIGITS_PER_CHANNEL, "%f", ); //TODO: take from pump manager
            memcpy(buffer[i].digits, temp, DIGITS_PER_CHANNEL);
        }
        //TODO: LEDs
    }
} // namespace display

_BEGIN_STD_C

void StartDisplayTask(void *argument)
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