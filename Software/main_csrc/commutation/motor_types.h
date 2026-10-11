// Defines useful types for motor control applications

#pragma once

typedef enum {
    PHASE_NULL = - 1,
    PHASE_A = 0,
    PHASE_B,
    PHASE_C,
    NO_PHASES,
} phase_idx_t;

typedef enum {
    //   HL
    STEP_NULL = -1,
    STEP_AB = 0,
    STEP_AC,
    STEP_BC,
    STEP_BA,
    STEP_CA,
    STEP_CB,
    NO_COMMUTATION_STEPS
} commutation_step_t;

