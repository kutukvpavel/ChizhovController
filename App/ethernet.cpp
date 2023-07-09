#include "ethernet.h"

#include "task_handles.h"
#include "spi_sync.h"
#include "wdt.h"
#include "../Drivers/ioLib/Ethernet/wizchip_conf.h"
#include "../Drivers/ioLib/Internet/DHCP/dhcp.h"

#define W5500_SPI_INDEX 1
#define EXPECTED_CHIP_VERSION 0x04

#define DHCP_SOCKET     0
#define DNS_SOCKET      1
#define HTTP_SOCKET     2

namespace eth
{
    // 1K should be enough, see https://forum.wiznet.io/t/topic/1612/2
    static uint8_t dhcp_buffer[1024];
    static wiz_NetInfo net_info = {
            .mac = {0xEA, 0x11, 0x22, 0x33, 0x44, 0xEA},
            .dhcp = NETINFO_DHCP};
    static HAL_StatusTypeDef last_err = HAL_OK;
    static uint32_t err_rate = 0;
    static uint8_t chip_ver = 0x00;

    void enter_critical()
    {
        portENTER_CRITICAL();
    }
    void exit_critical()
    {
        portEXIT_CRITICAL();
    }
    void W5500_Select() {
        while (spi::acquire_bus(W5500_SPI_INDEX) == HAL_BUSY) vTaskDelay(1);
    }
    void W5500_Unselect() {
        spi::release_bus();
    }
    void W5500_ReadBuff(uint8_t* buff, uint16_t len) {
        HAL_StatusTypeDef ret;
        if ((ret = spi::receive(buff, len)) != HAL_OK) ERR("SPI fail: %u", ret);
    }
    void W5500_WriteBuff(uint8_t* buff, uint16_t len) {
        HAL_StatusTypeDef ret;
        if ((ret = spi::transmit(buff, len)) != HAL_OK) ERR("SPI fail: %u", ret);
    }
    uint8_t W5500_ReadByte() {
        uint8_t byte;
        W5500_ReadBuff(&byte, sizeof(byte));
        return byte;
    }
    void W5500_WriteByte(uint8_t byte) {
        W5500_WriteBuff(&byte, sizeof(byte));
    }
    void Callback_IPAssigned() {
        DBG("Callback: IP assigned! Leased time: %lu sec\r\n", getDHCPLeasetime());
    }
    void Callback_IPConflict() {
        DBG("Callback: IP conflict!\r\n");
    }

    HAL_StatusTypeDef init()
    {
        reg_wizchip_cris_cbfunc(enter_critical, exit_critical);
        reg_wizchip_cs_cbfunc(W5500_Select, W5500_Unselect);
        reg_wizchip_spi_cbfunc(W5500_ReadByte, W5500_WriteByte);
        reg_wizchip_spiburst_cbfunc(W5500_ReadBuff, W5500_WriteBuff);

        wizchip_sw_reset();
        vTaskDelay(pdMS_TO_TICKS(10));
        chip_ver = getVERSIONR();
        if (chip_ver != EXPECTED_CHIP_VERSION)
        {
            ERR("Unexpected WizNet chip ver = %hu, should be = %hu", chip_ver, EXPECTED_CHIP_VERSION);
            return HAL_ERROR;
        }

        uint8_t rx_tx_buff_sizes[] = {2, 2, 2, 2, 2, 2, 2, 2};
        if (wizchip_init(rx_tx_buff_sizes, rx_tx_buff_sizes) != 0)
        {
            ERR("Failed to initialize WizNet chip");
            return HAL_ERROR;
        }

        // set MAC address before using DHCP
        setSHAR(net_info.mac);
        DHCP_init(DHCP_SOCKET, dhcp_buffer);
        reg_dhcp_cbfunc(
            Callback_IPAssigned,
            Callback_IPAssigned,
            Callback_IPConflict);

        return HAL_OK;
    }

    HAL_StatusTypeDef process_dhcp()
    {
        switch (DHCP_run())
        {
        case DHCP_FAILED:
        case DHCP_STOPPED:
            return HAL_ERROR;
        case DHCP_RUNNING:
        case DHCP_IP_LEASED:
            return HAL_OK;
        default:
            break;
        }

        getIPfromDHCP(net_info.ip);
        getGWfromDHCP(net_info.gw);
        getSNfromDHCP(net_info.sn);

        DBG("IP obtained");
        wizchip_setnetinfo(&net_info);

        return HAL_OK;
    }

    void print_dbg()
    {
        printf("\tChip ver = %hu\n"
            "\tLast err = %u\n"
            "\tErr rate = %lu\n",
            chip_ver,
            last_err,
            err_rate
        );
        printf("\tIP:  %d.%d.%d.%d\n\tGW:  %d.%d.%d.%d\n\tNet: %d.%d.%d.%d\n",
                    net_info.ip[0], net_info.ip[1], net_info.ip[2], net_info.ip[3],
                    net_info.gw[0], net_info.gw[1], net_info.gw[2], net_info.gw[3],
                    net_info.sn[0], net_info.sn[1], net_info.sn[2], net_info.sn[3]);
    }
    bool check_response(HAL_StatusTypeDef s)
    {
        if (s == HAL_OK) return true;
        last_err = s;
        err_rate++;
        return false;
    }
} // namespace eth

_BEGIN_STD_C
DECLARE_STATIC_TASK(MY_ETH)
{
    const TickType_t delay = pdMS_TO_TICKS(50);
    static wdt::task_t* pwdt;
    TickType_t last_wake = configINITIAL_TICK_COUNT;
    HAL_StatusTypeDef ret;
    DBG("Ethernet init...");
    ret = eth::init();
    if (ret == HAL_OK)
        DBG("\tEthernet init OK.");
    else
        ERR("\tEthernet init error: %u", ret);
    pwdt = wdt::register_task(2000, "ETH");
    INIT_NOTIFY(MY_ETH);

    if (ret == HAL_OK)
    {
        for (;;)
        {
            vTaskDelayUntil(&last_wake, delay);
            ret = eth::process_dhcp();
            eth::check_response(ret);

            pwdt->last_time = xTaskGetTickCount();
            if ((last_wake + delay) <= pwdt->last_time) last_wake = pwdt->last_time;
        }
    }
    else
    {
        for (;;)
        {
            vTaskDelay(delay);
            pwdt->last_time = xTaskGetTickCount();
        }
    }
}
_END_STD_C