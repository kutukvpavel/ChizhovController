#include "nvs.h"

#include "i2c.h"

#define MY_EEPROM_ADDR 0xA0
#define MY_NVS_I2C_ADDR(mem_addr) (MY_EEPROM_ADDR | ((mem_addr & 0x700) >> 7))
#define MY_NVS_VER_ADDR 0u
#define MY_NVS_START_ADDRESS 8u
#define MY_NVS_VERSION 0u
#define MY_NVS_PAGE_SIZE 8u
#define MY_NVS_TOTAL_PAGES 64u
#define MY_NVS_TOTAL_SIZE (MY_NVS_PAGE_SIZE * MY_NVS_TOTAL_PAGES)

namespace nvs
{
    struct storage_t
    {

    };
    static storage_t storage = {

    };

    uint8_t nvs_ver = 0;
    HAL_StatusTypeDef eeprom_read(uint16_t addr, uint8_t* buf, uint16_t len)
    {
        assert_param((addr + len) < MY_NVS_TOTAL_SIZE);
        return HAL_I2C_Mem_Read(&hi2c2, MY_NVS_I2C_ADDR(addr), addr & 0xFF, I2C_MEMADD_SIZE_8BIT, buf, len, 1000);
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
            status = HAL_I2C_Mem_Write(&hi2c2, MY_NVS_I2C_ADDR(current_page_addr), current_page_addr & 0xFF, I2C_MEMADD_SIZE_8BIT, 
                buf, MY_NVS_PAGE_SIZE, 1000);
            if (status != HAL_OK) break;
            buf += MY_NVS_PAGE_SIZE;
            current_page_addr += MY_NVS_PAGE_SIZE;
            HAL_Delay(5);
        }
        if (status != HAL_OK) return status;
        if (remainder > 0)
        {
            status = HAL_I2C_Mem_Write(&hi2c2, MY_NVS_I2C_ADDR(current_page_addr), current_page_addr & 0xFF, I2C_MEMADD_SIZE_8BIT, 
                buf, remainder, 1000);
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
} // namespace nvs