#include "user.h"
#include "sr_io.h"

#define REGULAR_REPAINT_DELAY_MS 50
#define MISSED_REPAINT_DELAY_MS 3
#define DISPLAY_CHANNELS 3

namespace display
{
    struct __packed single_channel_map
    {
        uint8_t digits[4];
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