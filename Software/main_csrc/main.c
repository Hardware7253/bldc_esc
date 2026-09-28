#include "stm32c0xx_hal.h"
#include "system.h"

int main(void) {
    HAL_Init();
    init_clocks();

    while (1) {
    
    }
    return 0;
}