// Gate driver module is for init and control of phase gate drivers

#pragma once

#include "motor_types.h"
#include <stdint.h>

void init_gd_peripherals(void);
void set_duty_100(uint8_t duty);
void set_duty_1000(uint16_t duty);
void switch_active_phases(commutation_step_t commutation_step);