#include "dfu.h"

#include "main.h"
#include "../../Drivers/STM32F4xx_HAL_Driver/Inc/stm32f4xx_ll_wwdg.h"

#define BOOTLOADER_ADDR (0x1FFF0000UL) //STM32F4

typedef struct
{
	uint32_t stackptr;
	void (*jumpptr)(void);
} bootloader_t;
static bootloader_t * const bootloader = (bootloader_t *)BOOTLOADER_ADDR;

void dfu_perform_wwdg_reset(void)
{
	__HAL_RCC_WWDG_CLK_ENABLE();
	LL_WWDG_SetPrescaler(WWDG, LL_WWDG_PRESCALER_1);
	LL_WWDG_SetWindow(WWDG, 0x40);
	LL_WWDG_SetCounter(WWDG, 0);
	LL_WWDG_Enable(WWDG);
}

// https://stm32f4-discovery.net/2017/04/tutorial-jump-system-memory-software-stm32/
// https://github.com/markusgritsch/SilF4ware
// https://community.st.com/t5/embedded-software-mcus/can-t-make-software-jump-to-bootloader-dfu-on-stm32f767/td-p/210209/page/2
void dfu_jump_to_bootloader( void )
{
	__HAL_SYSCFG_REMAPMEMORY_SYSTEMFLASH();
	__set_MSP(bootloader->stackptr);
	bootloader->jumpptr();
	NVIC_SystemReset();
}