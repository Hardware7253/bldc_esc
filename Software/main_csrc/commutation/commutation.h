// Commutation module is for changing and timing commutation steps
// using controlled or open-loop operation

#pragma once

#include "motor_types.h"
#include <stdbool.h>

bool did_commutation_just_step(void);
phase_idx_t get_floating_phase(void);
void blocking_ol_commutation(void);