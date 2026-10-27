#include "sample_raw.h"
#include "stm32c0xx_hal.h"
#include "system.h"
#include "motor_types.h"
#include "commutation.h"

// Clock enables for all sampling peripherals
#define SAMPLING_CLOCKS_ENABLE \
    __HAL_RCC_ADC_CLK_ENABLE(); \
    __HAL_RCC_DMA1_CLK_ENABLE(); \
    __HAL_RCC_GPIOA_CLK_ENABLE(); \
    __HAL_RCC_GPIOB_CLK_ENABLE


#define ADC_INSTANCE ADC1
#define ADC_SAMPLETIME ADC_SAMPLETIME_7CYCLES_5
static ADC_HandleTypeDef sampling_hadc = {0};

#define DMA_INSTANCE        DMA1_Channel1 
#define DMA_REQUEST         DMA_REQUEST_ADC1
#define DMA_IRQn            DMA1_Channel1_IRQn 
#define DMA_ISR             DMA1_Channel1_IRQHandler
static DMA_HandleTypeDef sampling_hdma = {0};

// Length of arrays used to configure each adc channel
// Index order is phase A, phase B, phase C, I sense
#define NO_ADC_CHANNELS (NO_PHASES + 1)

#define I_SENSE_IDX (PHASE_C + 1)

// Config for all ADC input pins
static GPIO_TypeDef* const ADC_PORTS[NO_ADC_CHANNELS] = {GPIOA, GPIOA, GPIOB, GPIOA};
static const uint32_t ADC_PINS[NO_ADC_CHANNELS] = {GPIO_PIN_5, GPIO_PIN_4, GPIO_PIN_1, GPIO_PIN_7};
static const uint32_t ADC_CHANNELS[NO_ADC_CHANNELS] = {ADC_CHANNEL_5, ADC_CHANNEL_4, ADC_CHANNEL_18, ADC_CHANNEL_7};

// Ranks set so sample_buf fills with phases in the expected order (A, B, C, I)
static const uint32_t ADC_RANKS[NO_ADC_CHANNELS] = {
    ADC_REGULAR_RANK_1,
    ADC_REGULAR_RANK_2,
    ADC_REGULAR_RANK_3,
    ADC_REGULAR_RANK_4
};

// Buffer for phase voltages and inverter current
static volatile uint16_t sample_buf[NO_ADC_CHANNELS] = {0};

static volatile uint16_t i_read = 0;
static volatile int32_t last_bemf_value = 0; // How far the last BEMF value was from the neutral point
static volatile bool edge_detected = false;  // True if an edge has already been detected during this comutation step


// Returns the last ADC read from the current shunt
uint16_t get_i_read(void) {
    return i_read;
}

// Initialise peripherals and start sampling with DMA
void start_sampling(void) {
    SAMPLING_CLOCKS_ENABLE();

    // Init pins
    {
        GPIO_InitTypeDef gpio_init = {
            .Mode = GPIO_MODE_ANALOG,
            .Pull = GPIO_NOPULL,
            .Speed = GPIO_SPEED_FREQ_LOW,
        };

        for (uint8_t i = 0; i < NO_PHASES; i++) {
            gpio_init.Pin = ADC_PINS[i];
            HAL_GPIO_Init(ADC_PORTS[i], &gpio_init);
        }
    }

    // Init ADC
    sampling_hadc.Instance = ADC_INSTANCE;
    sampling_hadc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
    sampling_hadc.Init.Resolution = ADC_RESOLUTION_12B;
    sampling_hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    sampling_hadc.Init.ScanConvMode = ADC_SCAN_ENABLE; // Use configurable sequencer
    sampling_hadc.Init.EOCSelection = ADC_EOC_SEQ_CONV; 
    sampling_hadc.Init.LowPowerAutoWait = DISABLE;
    sampling_hadc.Init.LowPowerAutoPowerOff = DISABLE;
    sampling_hadc.Init.ContinuousConvMode = DISABLE;
    sampling_hadc.Init.NbrOfConversion = 4;
    sampling_hadc.Init.DiscontinuousConvMode = DISABLE;
    sampling_hadc.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T1_CC4;
    sampling_hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    sampling_hadc.Init.DMAContinuousRequests = ENABLE;
    sampling_hadc.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
    sampling_hadc.Init.OversamplingMode = DISABLE;
    sampling_hadc.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_HIGH;
    error_handler_msg(HAL_ADC_Init(&sampling_hadc), "Failed to init ADC");

    // Init channels
    ADC_ChannelConfTypeDef channel_conf = {0};
    channel_conf.Rank = ADC_RANK_CHANNEL_NUMBER;
    channel_conf.SamplingTime = ADC_SAMPLETIME;
    for (uint8_t i = 0; i < NO_ADC_CHANNELS; i++) {
        channel_conf.Channel = ADC_CHANNELS[i];
        channel_conf.Rank = ADC_RANKS[i];
        error_handler_msg(HAL_ADC_ConfigChannel(&sampling_hadc, &channel_conf), "Failed to init channel");
    }

    // Init DMA
    HAL_NVIC_SetPriority(DMA_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA_IRQn);
    sampling_hdma.Instance = DMA_INSTANCE;
    sampling_hdma.Init.Request = DMA_REQUEST;
    sampling_hdma.Init.Direction = DMA_PERIPH_TO_MEMORY;
    sampling_hdma.Init.PeriphInc = DMA_PINC_DISABLE;
    sampling_hdma.Init.MemInc = DMA_MINC_ENABLE;
    sampling_hdma.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    sampling_hdma.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    sampling_hdma.Init.Mode = DMA_CIRCULAR;
    sampling_hdma.Init.Priority = DMA_PRIORITY_LOW;
    error_handler_msg(HAL_DMA_Init(&sampling_hdma), "Failed to init DMA");

    __HAL_LINKDMA(&sampling_hadc, DMA_Handle, sampling_hdma);

    // Start reading
    HAL_ADCEx_Calibration_Start(&sampling_hadc);
    error_handler_msg(HAL_ADC_Start_DMA(&sampling_hadc, (uint32_t*)sample_buf, NO_ADC_CHANNELS), "Failed to start ADC");

    GPIO_InitTypeDef gpio_init = {
        .Pin = GPIO_PIN_2,
        .Mode = GPIO_MODE_OUTPUT_PP,
        .Pull = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_LOW
    };
    HAL_GPIO_Init(GPIOA, &gpio_init);
}

// When the buffer has filled process the values
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET);
    if (hadc->Instance != ADC_INSTANCE)
        return;

    i_read = sample_buf[I_SENSE_IDX];
    if (did_commutation_just_step())
        edge_detected = false;

    uint16_t neutral = (sample_buf[PHASE_A] + sample_buf[PHASE_B] + sample_buf[PHASE_C]) / 3;
    int32_t bemf_value = (int32_t)sample_buf[get_floating_phase()] - (int32_t)neutral;

    if (
        ( // Detect raw zero crossing
            bemf_value == 0 ||
            (bemf_value > 0 && last_bemf_value < 0) ||
            (bemf_value < 0 && last_bemf_value > 0)
        ) &&

        // Check there wasn't already a zero crossing detected this commmutation step
        !edge_detected
    ) {
        edge_detected = true;

        // Check zero crossing occurs roughly where it's expected (halfway through current commutation step)
    }

    // last_bemf_value = bemf_value;
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
}

extern void DMA_ISR(void) {
    HAL_DMA_IRQHandler(&sampling_hdma);
}