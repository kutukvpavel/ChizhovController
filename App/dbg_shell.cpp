#include "user.h"

#include "ushell/inc/sys_command_line.h"
#include "../Core/Inc/usart.h"

static void init();

_BEGIN_STD_C

void StartDebugMenuTask(void *argument)
{
    //Init debug commands
    init();

    //run loop
    for (;;)
    {
        CLI_RUN();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

_END_STD_C

namespace cli_commands
{
    uint8_t info(int argc, char** argv)
    {
        puts(MY_FIRMWARE_INFO_STR);
    }
} // namespace cli_commands

void init()
{
    CLI_INIT(&huart1);

    CLI_ADD_CMD("info", "Get device info", &cli_commands::info);
}