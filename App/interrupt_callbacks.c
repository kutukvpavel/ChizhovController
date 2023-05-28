#include "user.h"
#include "./modbus/MODBUS-LIB/Inc/Modbus.h"
#include "sys_command_line.h"
#include "task_handles.h"

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
	modbus_uart_txcplt_callback(huart);
	cli_uart_txcplt_callback(huart);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	modbus_uart_rxcplt_callback(huart);
	cli_uart_rxcplt_callback(huart);
}

//Fires after a DMA transfer of a result in DMA mode (DMA interrupt calls this callback handler)
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc)
{
    if (hadc->Instance != ADC1) return;

	BaseType_t woken;
    vTaskNotifyGiveFromISR(STATIC_TASK_HANDLE(MY_ADC), &woken);
	portYIELD_FROM_ISR(woken);
}