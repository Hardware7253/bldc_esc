// Gate driver module is for initialising peripherals
// related to the phase half-bridge gate drivers

#pragma once

#include "stm32c0xx_hal.h"
#include <stdint.h>

void init_gd_peripherals(void);
void set_duty_100(uint8_t duty);
void set_duty_1000(uint16_t duty);
void test(void);