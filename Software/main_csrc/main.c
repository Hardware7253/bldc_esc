#include "stm32c0xx_hal.h"
#include "system.h"
#include "gate_drive.h"

int main(void) {
    HAL_Init();
    init_clocks();
    init_gd_peripherals();

    while (1) {
    }
    return 0;
}