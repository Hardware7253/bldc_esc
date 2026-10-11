// Commutation module is for changing and timing commutation steps
// using controlled or open-loop operation

#pragma once

#include "motor_types.h"

phase_idx_t get_floating_phase();
void blocking_ol_commutation(void);