#include "stm32c0xx_hal.h"
#include "system.h"
#include "commutation.h"
#include "gate_drive.h"

int main(void) {
    HAL_Init();
    init_clocks();
    init_gd_peripherals();

    while (1) {
        switch_active_phases(STEP_AB);
        HAL_Delay(1);
        switch_active_phases(STEP_AC);
        HAL_Delay(1);
        switch_active_phases(STEP_BC);
        HAL_Delay(1);
        switch_active_phases(STEP_BA);
        HAL_Delay(1);
        switch_active_phases(STEP_CA);
        HAL_Delay(1);
        switch_active_phases(STEP_CB);
        HAL_Delay(1);
    }
    return 0;
}