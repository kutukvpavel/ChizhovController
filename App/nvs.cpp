#include "nvs.h"

#include "i2c_sync.h"

#define MY_EEPROM_ADDR 0xA0
#define MY_NVS_I2C_ADDR(mem_addr) (MY_EEPROM_ADDR | ((mem_addr & 0x700) >> 7))
#define MY_NVS_VER_ADDR 0u
#define MY_NVS_START_ADDRESS 8u
#define MY_NVS_VERSION 0u
#define MY_NVS_PAGE_SIZE 8u
#define MY_NVS_TOTAL_PAGES 64u
#define MY_NVS_TOTAL_SIZE (MY_NVS_PAGE_SIZE * MY_NVS_TOTAL_PAGES)
#define MY_RETRIES 3
#define PACKED_FOR_NVS __packed __aligned(sizeof(float))

namespace nvs
{
    struct PACKED_FOR_NVS storage_t
    {
        pumps::params_t pump_params;
        motor_params_t motor_params[MY_PUMPS_MAX];
        motor_reg_t motor_regs[MY_PUMPS_MAX];
        uint16_t modbus_id;
    };
    static storage_t storage = {
        .pump_params = {
            .invert_enable = 1
        },
        .motor_params = {
            {
                .dir = 0,
                .microsteps = 32,
                .teeth = 200,
                .reserved1 = 0, //alignment
                .volume_rate_to_rps = 1,
                .max_rate_rps = 20,
                .max_load_err = 17
            },
            {
                .dir = 0,
                .microsteps = 32,
                .teeth = 200,
                .reserved1 = 0, //alignment
                .volume_rate_to_rps = 1,
                .max_rate_rps = 20,
                .max_load_err = 17
            },
            {
                .dir = 0,
                .microsteps = 32,
                .teeth = 200,
                .reserved1 = 0, //alignment
                .volume_rate_to_rps = 1,
                .max_rate_rps = 20,
                .max_load_err = 17
            }
        },
        .motor_regs = {
            {
                .volume_rate = 0,
                .rps = 0,
                .err = 0
            },
            {
                .volume_rate = 0,
                .rps = 0,
                .err = 0
            },
            {
                .volume_rate = 0,
                .rps = 0,
                .err = 0
            }
        },
        .modbus_id = 1
    };

    static uint8_t nvs_ver = 0;
    HAL_StatusTypeDef eeprom_read(uint16_t addr, uint8_t* buf, uint16_t len)
    {
        assert_param((addr + len) < MY_NVS_TOTAL_SIZE);
        
        HAL_StatusTypeDef ret = HAL_ERROR;
        int retries = MY_RETRIES;
        while (retries--)
        {
            ret = i2c::mem_read(MY_NVS_I2C_ADDR(addr), addr & 0xFF, buf, len);
            if (ret == HAL_OK) break;
        }
        return ret;
    }
    HAL_StatusTypeDef eeprom_write(uint16_t addr, uint8_t* buf, uint16_t len)
    {
        assert_param((addr + len) < MY_NVS_TOTAL_SIZE);
        assert_param(addr % MY_NVS_PAGE_SIZE == 0);
        HAL_StatusTypeDef status = HAL_OK;

        size_t full_pages = len / MY_NVS_PAGE_SIZE;
        size_t remainder = len % MY_NVS_PAGE_SIZE;
        DBG("Writing NVS: Full pages = %u, Remainder = %u", full_pages, remainder);
        uint16_t current_page_addr = addr;
        for (size_t i = 0; i < full_pages; i++)
        {
            status = i2c::mem_write(MY_NVS_I2C_ADDR(current_page_addr), current_page_addr & 0xFF, buf, MY_NVS_PAGE_SIZE);
            if (status != HAL_OK) break;
            buf += MY_NVS_PAGE_SIZE;
            current_page_addr += MY_NVS_PAGE_SIZE;
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        if (status != HAL_OK) return status;
        if (remainder > 0)
        {
            status = i2c::mem_write(MY_NVS_I2C_ADDR(current_page_addr), current_page_addr & 0xFF, buf, remainder);
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        return status;
    }

    HAL_StatusTypeDef init()
    {
        static_assert(MY_NVS_START_ADDRESS % MY_NVS_PAGE_SIZE == 0);
        HAL_StatusTypeDef ret;
        DBG("NVS init...");

        if ((ret = eeprom_read(MY_NVS_VER_ADDR, &nvs_ver, sizeof(nvs_ver))) != HAL_OK) return ret;
        DBG("Detected NVS ver: %u", nvs_ver);
        if (nvs_ver != MY_NVS_VERSION) return HAL_ERROR;
        return HAL_OK;
    }
    HAL_StatusTypeDef load()
    {
        HAL_StatusTypeDef ret;

        if (nvs_ver != MY_NVS_VERSION) return HAL_ERROR;
        if ((ret = eeprom_read(MY_NVS_START_ADDRESS, reinterpret_cast<uint8_t*>(&storage), sizeof(storage))) != HAL_OK) return ret;
        return HAL_OK;
    }
    HAL_StatusTypeDef save()
    {
        HAL_StatusTypeDef ret;
        
        if ((ret = eeprom_write(MY_NVS_START_ADDRESS, reinterpret_cast<uint8_t*>(&storage), sizeof(storage))) != HAL_OK) return ret;
        DBG("NVS Data written OK. Writing NVS version...");
        uint8_t ver[] = { MY_NVS_VERSION };
        return eeprom_write(MY_NVS_VER_ADDR, ver, 1);
    }
    HAL_StatusTypeDef reset()
    {
        uint8_t zero[] = { 0 };
        return eeprom_write(MY_NVS_VER_ADDR, zero, 1);
    }
    void dump_hex()
    {
        for (size_t i = 0; i < sizeof(storage); i++)
        {
            printf("0x%02X\n", reinterpret_cast<uint8_t*>(&storage)[i]);
        }
    }
    HAL_StatusTypeDef test()
    {
        uint8_t* sequential_buffer = new uint8_t[sizeof(storage_t)];
        assert_param(sequential_buffer);

        for (size_t i = 0; i < sizeof(storage_t); i++)
        {
            sequential_buffer[i] = static_cast<uint8_t>(i);
        }
        eeprom_write(MY_NVS_START_ADDRESS, sequential_buffer, sizeof(storage_t));
        HAL_Delay(5);

        uint8_t* readback_buffer = new uint8_t[sizeof(storage_t)];
        assert_param(readback_buffer);
        eeprom_read(MY_NVS_START_ADDRESS, readback_buffer, sizeof(storage_t));
        HAL_StatusTypeDef res = (memcmp(sequential_buffer, readback_buffer, sizeof(storage_t)) == 0) ? HAL_OK : HAL_ERROR;

        delete[] sequential_buffer;
        HAL_Delay(5);
        save();
        return res;
    }

    motor_reg_t* get_motor_regs()
    {
        return &(storage.motor_regs[0]);
    }
    motor_params_t* get_motor_params()
    {
        return &(storage.motor_params[0]);
    }
    pumps::params_t* get_pump_params()
    {
        return &(storage.pump_params);
    }
    uint16_t* get_modbus_addr()
    {
        return &(storage.modbus_id);
    }
} // namespace nvs
