#include "gate_drive.h"
#include "stm32c0xx_hal.h"
#include "system.h"
#include "map.h"

static TIM_HandleTypeDef htim1;
static TIM_HandleTypeDef htim3;

#define PERIPH_CLK_ENABLE \
    __HAL_RCC_GPIOA_CLK_ENABLE(); \
    __HAL_RCC_GPIOB_CLK_ENABLE(); \
    __HAL_RCC_TIM1_CLK_ENABLE(); \
    __HAL_RCC_TIM3_CLK_ENABLE


#define NO_TIMS 2
static TIM_HandleTypeDef* const TIM_HANDLES[NO_TIMS] = {&htim1, &htim3};
static TIM_TypeDef* const TIM_INSTANCES[NO_TIMS] = {TIM1, TIM3};

#define NO_PHASES PHASE_NULL
typedef enum {
    PHASE_A = 0,
    PHASE_B,
    PHASE_C,
    PHASE_NULL,
} phase_idx_t;

// Peripheral arrays indexed by phase_idx_t
static GPIO_TypeDef* const LOW_PORTS[NO_PHASES] = {GPIOB, GPIOA, GPIOA};
static GPIO_TypeDef* const HIGH_PORTS[NO_PHASES] = {GPIOB, GPIOA, GPIOA};
static const uint32_t LOW_PINS[NO_PHASES] = {GPIO_PIN_4, GPIO_PIN_10, GPIO_PIN_8};
static const uint32_t HIGH_PINS[NO_PHASES] = {GPIO_PIN_5, GPIO_PIN_11, GPIO_PIN_9};
static const uint32_t HIGH_AFS[NO_PHASES] = {GPIO_AF1_TIM3, GPIO_AF2_TIM1, GPIO_AF2_TIM1};
static TIM_HandleTypeDef* const HIGH_TIM_HANDLES[NO_PHASES] = {&htim3, &htim1, &htim1};
static const uint32_t HIGH_TIM_CHANS[NO_PHASES] = {TIM_CHANNEL_2, TIM_CHANNEL_4, TIM_CHANNEL_2};


// Base clock = 48MHz
// Prescaler and period set for 100kHz output
const uint16_t TIM_PRESCALER = 0;
const uint16_t TIM_PERIOD = 479;

// Currently active phases
static phase_idx_t high_phase = PHASE_NULL;
static phase_idx_t low_phase = PHASE_NULL;

void init_gd_peripherals(void) {
    PERIPH_CLK_ENABLE();

    GPIO_InitTypeDef gpio_init = {
        .Mode = GPIO_MODE_OUTPUT_PP,
        .Pull = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_LOW,
    };

    // Init low-side gate control pins
    for (uint8_t i = 0; i < NO_PHASES; i++) {
        gpio_init.Pin = LOW_PINS[i];
        HAL_GPIO_Init(LOW_PORTS[i], &gpio_init);
    }

    // Init high-side gate control pins
    gpio_init.Pull = GPIO_PULLDOWN;
    gpio_init.Mode = GPIO_MODE_AF_PP;
    for (uint8_t i = 0; i < NO_PHASES; i++) {
        gpio_init.Pin = HIGH_PINS[i];
        gpio_init.Alternate = HIGH_AFS[i];
        HAL_GPIO_Init(HIGH_PORTS[i], &gpio_init);
    }

    // PWM configs
    TIM_Base_InitTypeDef tim_init = {
        .Prescaler = TIM_PRESCALER,
        .Period = TIM_PERIOD,
        .CounterMode = TIM_COUNTERMODE_UP,
        .ClockDivision = TIM_CLOCKDIVISION_DIV1,
        .RepetitionCounter = 0,
        .AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE,
    };

    TIM_OC_InitTypeDef tim_oc_init = {
        .OCMode = TIM_OCMODE_PWM2,
        .Pulse = TIM_PERIOD / 2, // Start with 50% duty ratio
        .OCPolarity = TIM_OCPOLARITY_LOW,
        .OCFastMode = TIM_OCFAST_DISABLE,
        .OCIdleState = TIM_OCIDLESTATE_RESET,
    };

    TIM_ClockConfigTypeDef clock_cfg = {
        .ClockSource = TIM_CLOCKSOURCE_INTERNAL,
        .ClockPolarity = TIM_CLOCKPOLARITY_NONINVERTED,
        .ClockPrescaler = TIM_CLOCKPRESCALER_DIV1,
        .ClockFilter = 0,
    };

    TIM_MasterConfigTypeDef master_cfg = {0};
    master_cfg.MasterOutputTrigger = TIM_TRGO_RESET;
    master_cfg.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;

    // Init high side PWM timers
    for (uint8_t i = 0; i < NO_TIMS; i++) {
        TIM_HandleTypeDef* htim = TIM_HANDLES[i];
        htim->Instance = TIM_INSTANCES[i];
        htim->Init = tim_init;

        error_handler(HAL_TIM_Base_Init(htim));
        error_handler(HAL_TIM_ConfigClockSource(htim, &clock_cfg));
        error_handler(HAL_TIMEx_MasterConfigSynchronization(htim, &master_cfg));
        error_handler(HAL_TIM_PWM_Init(htim));
    }

    // Configure pwm channels
    for (uint8_t i = 0; i < NO_PHASES; i++) {
        TIM_HandleTypeDef* htim = HIGH_TIM_HANDLES[i];
        uint32_t tim_channel = HIGH_TIM_CHANS[i];
        error_handler(HAL_TIM_PWM_ConfigChannel(htim, &tim_oc_init, tim_channel));
    }
}

// Dead time for driving inverter gates
// Currently unused because switching waveforms get some natural dead time from HAL delays
// Usually get ~1uS dead time with STM32C031 at 48MHz
// Faster MCU's will likely require extra dead time
static void dead_time(void) {}

// Switches the low side and starts PWM of high side for the given commutation step
// Also switches off the old low side and high side and inserts dead time
void switch_active_phases(commutation_step_t commutation_step) {
    phase_idx_t new_high_phase = PHASE_NULL;
    phase_idx_t new_low_phase = PHASE_NULL;

    switch (commutation_step) {
        case STEP_AB:
            new_high_phase = PHASE_A;
            new_low_phase  = PHASE_B;
            break;

        case STEP_AC:
            new_high_phase = PHASE_A;
            new_low_phase  = PHASE_C;
            break;

        case STEP_BC:
            new_high_phase = PHASE_B;
            new_low_phase  = PHASE_C;
            break;

        case STEP_BA:
            new_high_phase = PHASE_B;
            new_low_phase  = PHASE_A;
            break;

        case STEP_CA:
            new_high_phase = PHASE_C;
            new_low_phase  = PHASE_A;
            break;

        case STEP_CB:
            new_high_phase = PHASE_C;
            new_low_phase  = PHASE_B;
            break;

        default:
            break;
    }

    // Turn off old high phase and turn on new high phase
    if (new_high_phase != high_phase) {
        if (high_phase != PHASE_NULL) {
            error_handler(HAL_TIM_PWM_Stop(HIGH_TIM_HANDLES[high_phase], HIGH_TIM_CHANS[high_phase]));
            dead_time();
        }

        error_handler(HAL_TIM_PWM_Start(HIGH_TIM_HANDLES[new_high_phase], HIGH_TIM_CHANS[new_high_phase]));
    }

    // Turn off old low phase and turn on new low phase
    if (new_low_phase != low_phase) {
        if (low_phase != PHASE_NULL) {
            HAL_GPIO_WritePin(LOW_PORTS[low_phase], LOW_PINS[low_phase], GPIO_PIN_RESET);
            dead_time();
        }
        HAL_GPIO_WritePin(LOW_PORTS[new_low_phase], LOW_PINS[new_low_phase], GPIO_PIN_SET);
    }

    high_phase = new_high_phase;
    low_phase = new_low_phase;
}


// Sets the duty ratio of the high side phase PWM
// Duty can be [0, 100] where 100 corresponds to 100% duty ratio
void set_duty_100(uint8_t duty) {
    set_duty_1000((uint16_t)duty * 10);
}

// Sets the duty ratio of the high side phase PWM
// Duty can be [0, 1000] where 1000 corresponds to 100% duty ratio
// This function offers better resolution that set_duty_100
void set_duty_1000(uint16_t duty) {
    uint16_t duty_ccr = (uint16_t)map((int32_t)duty, 0, 1000, 0, (int32_t)TIM_PERIOD);
    for (uint8_t i = 0; i < NO_PHASES; i++) {
        TIM_HandleTypeDef* htim = HIGH_TIM_HANDLES[i];
        __HAL_TIM_SET_COMPARE(htim, HIGH_TIM_CHANS[i], duty_ccr);
    }
}