#include "user.h"

#include "ushell/inc/sys_command_line.h"
#include "../Core/Inc/usart.h"
#include "task_handles.h"
#include "wdt.h"
#include "nvs.h"
#include "coprocessor.h"
#include "i2c_sync.h"
#include "sr_io.h"
#include "a_io.h"
#include "pumps.h"
#include "thermo.h"

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
    uint8_t peripherals_report(int argc, char** argv)
    {
        fputs("\tSR_IO state:\n\t\tInputs = 0b ", stdout);
        for (size_t i = 0; i < sr_io::in::IN_LEN; i++)
        {
            putc(sr_io::get_input(static_cast<sr_io::in>(sr_io::in::IN_LEN - i - 1)) ? '1' : '0', stdout);
            if (i % 8 == 7) putc(' ', stdout);
        }
        fputs("\n\t\tOutputs = 0b ", stdout);
        for (size_t i = 0; i < sr_io::out::OUT_LEN; i++)
        {
            putc(sr_io::get_output(static_cast<sr_io::out>(sr_io::out::OUT_LEN - i - 1)) ? '1' : '0', stdout);
            if (i % 8 == 7) putc(' ', stdout);
        }
        printf("\n\tA_IO state:\n"
            "\t\tAmbient = %f\n"
            "\t\tThermocouple = %f\n"
            "\t\tVbat = %f\n"
            "\t\tVref = %f\n",
            a_io::get_input(a_io::in::ambient_temp),
            a_io::get_input(a_io::in::analog_thermocouple),
            a_io::get_input(a_io::in::vbat),
            a_io::get_input(a_io::in::vref));
        puts("\tMAX6675 Thermocouple state:");
        for (size_t i = 0; i < MY_TEMP_CHANNEL_NUM; i++)
        {
            printf("\t\t#%u = %f, present = %u\n", i, thermo::get_temperatures()[i], thermo::get_thermocouple_present(i));
        }
        return 0;
    }
    uint8_t pumps_report(int argc, char** argv)
    {
        printf("\tPumps enabled: %u\n", pumps::get_enabled());
        for (size_t i = 0; i < MY_PUMPS_NUM; i++)
        {
            printf("\tUnit #%u:\n"
                "\t\tMissing: %u\n"
                "\t\tOverload: %u\n"
                "\t\tIndicated speed = %f\n"
                "\t\tSpeed fraction = %f\n"
                "\t\tLoad fraction = %f\n"
                "\t\tPaused: %u\n",
                i,
                pumps::get_missing(i),
                pumps::get_overload(i),
                pumps::get_indicated_speed(i),
                pumps::get_speed_fraction(i),
                pumps::get_load_fraction(i),
                pumps::get_paused(i));
        }
        return 0;
    }
    uint8_t coproc_report(int argc, char** argv)
    {
        printf("\tManual override = %f\n", coprocessor::get_manual_override());
        for (size_t i = 0; i < MY_PUMPS_NUM; i++)
        {
            printf("\tUnit #%u:\n"
                "\t\tDrv missing: %u\n"
                "\t\tDrv error: %u\n"
                "\t\tEncoder position = %u\n"
                "\t\tEncoder btn pressed: %u\n",
                i,
                coprocessor::get_drv_missing(i),
                coprocessor::get_drv_error(i),
                coprocessor::get_encoder_value(i),
                coprocessor::get_encoder_btn_pressed(i));
        }
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
    uint8_t get_thermo_err_rate(int argc, char** argv)
    {
        printf("\t%lu\n", thermo::get_recv_err_rate());
        return 0;
    }

    void print_input_invert()
    {
        sr_buf_t* inv = nvs::get_input_inversion();
        for (size_t i = 0; i < sr_io::input_buffer_len; i++)
        {
            printf("\tWord #%u = 0x%X = 0b ", i, inv[i]);
            for (size_t j = 0; j < (sizeof(sr_buf_t) * __CHAR_BIT__); j++)
            {
                putc((inv[i] & (1u << (sizeof(sr_buf_t) * __CHAR_BIT__ - j - 1))) ? '1' : '0', stdout);
                if (j % 8 == 7) putc(' ', stdout);
            }
            putc('\n', stdout);
        }
    }
    uint8_t input_invert(int argc, char** argv)
    {
        if (argc < 2)
        {
            print_input_invert();
            return 0;
        }
        
        sr_buf_t* inv = nvs::get_input_inversion();
        if (argc == 2)
        {
            size_t idx;
            if (sscanf(argv[1], "%u", &idx) != 1) return 2;
            *inv ^= (1u << idx);
        }
        else if (argc == (sr_io::input_buffer_len + 1))
        {
            sr_buf_t b[sr_io::input_buffer_len];
            for (size_t i = 0; i < sr_io::input_buffer_len; i++)
            {
                if (sscanf(argv[i + 1], "%hx", &(b[i])) != 1) return 2;
            }
            memcpy(inv, b, sizeof(b));
        }
        else
        {
            return 1;
        }

        print_input_invert();
        return 0;
    }
} // namespace cli_commands

void init()
{
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    CLI_INIT(&huart1);

    CLI_ADD_CMD("info", "Get device info", &cli_commands::info);

    CLI_ADD_CMD("periph_report", "Report peripheral state", &cli_commands::peripherals_report);
    CLI_ADD_CMD("coproc_report", "Report coprocessor-controled devices' state", &cli_commands::coproc_report);
    CLI_ADD_CMD("pumps_report", "Report pump state", &cli_commands::pumps_report);

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

    CLI_ADD_CMD("get_thermo_err_rate", "Get MAX6675 SPI RX error count since boot", &cli_commands::get_thermo_err_rate);

    CLI_ADD_CMD("input_invert", "Get/set SR_IO input inversion register. No args = print current, "
        "1 arg = index of the input to toggle inversion bit for, "
        "N>2 args = N inv register words in hex without 0x (number of words is printed with no args given)",
        &cli_commands::input_invert);
}