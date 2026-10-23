#include "stm32c0xx_hal.h"
#include "system.h"
#include "commutation.h"
#include "gate_drive.h"

int main(void) {
    HAL_Init();
    init_clocks();
    init_gd_peripherals();

    while (1) {
        set_duty_100(20);
        blocking_ol_commutation();
    }
    return 0;
}