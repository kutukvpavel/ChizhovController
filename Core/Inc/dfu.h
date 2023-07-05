#pragma once

#include "stdlib.h"

_BEGIN_STD_C

void dfu_perform_wwdg_reset(void);
void dfu_jump_to_bootloader( void );

_END_STD_C