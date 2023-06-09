#include "user.h"

#include "ushell/inc/sys_command_line.h"
#include "../Core/Inc/usart.h"
#include "task_handles.h"
#include "wdt.h"
#include "nvs.h"
#include "coprocessor.h"
#include "i2c_sync.h"

static void init();

_BEGIN_STD_C
STATIC_TASK_BODY(MY_CLI)
{
    static wdt::task_t* pwdt;

    //Init debug commands
    init();
    pwdt = wdt::register_task(1000, "dbg");
    INIT_NOTIFY(MY_CLI);

    //run loop
    for (;;)
    {
        pwdt->last_time = xTaskGetTickCount();
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

    uint8_t nvs_save(int argc, char** argv)
    {
        return nvs::save();
    }
    uint8_t nvs_reset(int argc, char** argv)
    {
        return nvs::reset();
    }
    uint8_t nvs_test(int argc, char** argv)
    {
        return nvs::test();
    }
    uint8_t nvs_load(int argc, char** argv)
    {
        return nvs::load();
    }
    uint8_t nvs_dump(int argc, char** argv)
    {
        nvs::dump_hex();
        return 0;
    }

    uint8_t get_coproc_err_rate(int argc, char** argv)
    {
        printf("\t%lu\n", coprocessor::get_crc_err_stats());
        return 0;
    }
    uint8_t get_coproc_init(int argc, char** argv)
    {
        puts(coprocessor::get_initialized() ? "\tYes" : "\tNo");
        return 0;
    }
    uint8_t coproc_reinit(int argc, char** argv)
    {
        return coprocessor::reinit();
    }
    uint8_t coproc_scan(int argc, char** argv)
    {
        for (size_t i = 0x08; i < 0x7F; i++)
        {
            HAL_StatusTypeDef ret = i2c::write_byte(i << 1u, 0xA0);
            if (ret == HAL_BUSY) ERR("I2C synchronizing context is busy.");
            if (ret == HAL_OK) printf("\tFound: 0x%x\n", i);
        }
        return 0;
    }
} // namespace cli_commands

void init()
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    CLI_INIT(&huart1);

    CLI_ADD_CMD("info", "Get device info", &cli_commands::info);
    CLI_ADD_CMD("nvs_save", "Save current non-volatile data into EEPROM", &cli_commands::nvs_save);
    CLI_ADD_CMD("nvs_load", "Load non-volatile data from EEPROM", &cli_commands::nvs_load);
    CLI_ADD_CMD("nvs_dump", "Perform EEPROM HEX-dump", &cli_commands::nvs_dump);
    CLI_ADD_CMD("nvs_reset", "Reset NVS (sets NVS partiton version to 0 [invalid], doesn't actually erase the EEPROM)",
        &cli_commands::nvs_reset);
    CLI_ADD_CMD("nvs_test", "Test EEPROM readback, performs sequential number write and read, and does nvs_save afterwards",
        &cli_commands::nvs_test);
    CLI_ADD_CMD("get_coproc_err_rate", "Print coprocessor CRC error count since boot",
        &cli_commands::get_coproc_err_rate);
    CLI_ADD_CMD("get_coproc_init", "Prints whether coprocessor init-byte has been sent",
        &cli_commands::get_coproc_init);
    CLI_ADD_CMD("coproc_reinit", "Try to resend init-byte and read coprocessor afterwards",
        &cli_commands::coproc_reinit);
    CLI_ADD_CMD("coproc_scan", "Scan I2C bus by trying to write to addresses from 0x08 to 0x7F. Prints only found devs or HAL_BUSY",
        &cli_commands::coproc_scan);
}