// Gate driver module is for init and control of phase gate drivers

#pragma once

#include "motor_types.h"
#include <stdint.h>

void init_gd_peripherals(void);
void switch_active_phases(commutation_step_t commutation_step);


// Min Duty ratio should be 10% so ADC has sufficient time to sample during on period
// Max duty ratio should be ~95% so high side gate drive bootstrap capacitor can charge
void set_duty_100(uint16_t duty);
void set_duty_1000(uint16_t duty);