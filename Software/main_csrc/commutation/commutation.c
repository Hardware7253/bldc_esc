#include "commutation.h"
#include "gate_drive.h"
#include "stm32c0xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

static bool forward_rotation = true; 
static commutation_step_t current_step = STEP_NULL; 
static phase_idx_t floating_phase = PHASE_NULL;

// Delay between open-loop commutation steps
static int16_t ol_step_delay = 100; // ms

static const int16_t ol_ramp_step = 4;      // How much to decrement by after each electrical cycle
static const int16_t ol_step_target = 1;    // Final steady-state target

// Get the floating phase using commutation_step_t as an index
static const phase_idx_t FLOATING_PHASE_LUT[NO_COMMUTATION_STEPS] = {PHASE_C, PHASE_B, PHASE_A, PHASE_C, PHASE_B, PHASE_A};

phase_idx_t get_floating_phase() {
    return floating_phase;
}

// Changes active inverter phase pair to the next step
void next_step(void) {
    if (forward_rotation) {
        current_step++;
    } else {
        current_step--;
    }

    // Wrap around counting
    if (current_step == STEP_NULL)
        current_step = NO_COMMUTATION_STEPS - 1;
    else if(current_step == NO_COMMUTATION_STEPS)
        current_step = STEP_NULL + 1; 
    
    floating_phase = FLOATING_PHASE_LUT[current_step];
    switch_active_phases(current_step);
}

// Blocking open-loop commutation
// Blocks for a full electrical cycle
void blocking_ol_commutation(void) {
    for (int i = 0; i < NO_COMMUTATION_STEPS; i++) {
        HAL_Delay((uint32_t)ol_step_delay);
        next_step();
    }

    // Decrement and clamp delay
    ol_step_delay -= ol_ramp_step;
    if (ol_step_delay < ol_step_target) {
        ol_step_delay = ol_step_target;
    }
}

