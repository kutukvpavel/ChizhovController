#include "dbg_shell.h"

#include "user.h"
#include "ushell/inc/sys_command_line.h"
#include "../Core/Inc/usart.h"
#include "task_handles.h"

static void init();

namespace cli
{
    static bool ready_buf = false;

    const bool* ready = &ready_buf;
} // namespace cli


_BEGIN_STD_C
STATIC_TASK_BODY(MY_CLI)
{
    //Init debug commands
    init();
    cli::ready_buf = true;

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
        return 0;
    }
} // namespace cli_commands

void init()
{
    CLI_INIT(&huart1);

    CLI_ADD_CMD("info", "Get device info", &cli_commands::info);
}