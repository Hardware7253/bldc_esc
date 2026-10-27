#include "stm32c0xx_hal.h"
#include "system.h"
#include "commutation.h"
#include "gate_drive.h"
#include "sample_raw.h"

int main(void) {
    HAL_Init();
    init_clocks();
    init_gd_peripherals();
    start_sampling();

    while (1) {
        set_duty_100(15);
        blocking_ol_commutation();
    }
    return 0;
}