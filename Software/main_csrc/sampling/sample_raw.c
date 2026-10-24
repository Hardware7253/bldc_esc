#include "sample_raw.h"
#include "stm32c0xx_hal.h"
#include "system.h"
#include "motor_types.h"

// Clock enables for all sampling peripherals
#define SAMPLING_CLOCKS_ENABLE \
    __HAL_RCC_ADC_CLK_ENABLE(); \
    __HAL_RCC_DMA1_CLK_ENABLE(); \
    __HAL_RCC_GPIOA_CLK_ENABLE(); \
    __HAL_RCC_GPIOB_CLK_ENABLE


#define ADC_INSTANCE ADC1
static ADC_HandleTypeDef sampling_hadc = {0};

#define DMA_INSTANCE        DMA1_Channel1 
#define DMA_REQUEST         DMA_REQUEST_ADC1
#define DMA_IRQn            DMA1_Channel1_IRQn 
#define DMA_ISR             DMA1_Channel1_IRQHandler
static DMA_HandleTypeDef sampling_hdma = {0};

// Length of arrays used to configure each adc channel
// Index order is phase A, phase B, phase C, I sense
#define NO_ADC_CHANNELS (NO_PHASES + 1)

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

// Double buffer for phase voltages and inverter current
static volatile uint16_t sample_buf[2][NO_ADC_CHANNELS] = {0};
static volatile uint8_t full_index = 1;          // Index of the buffer that can be read [0, 1], this has to start at 1
static volatile bool buffer_ready = false;       // Set true by the ISR when a new buffer is ready to be read

// Returns true when a buffer is first ready
bool is_buffer_ready(void) {
    if (buffer_ready) {
        buffer_ready = false;
        return true;
    }
    return false;
}

// Returns a pointer to the buffer that is not currently being filled
// Can be indexed by phase A, B, C
// Also index 3 is the I sense value
inline uint16_t* get_raw_buf(void) {
    return (uint16_t*)(sample_buf + (full_index) * NO_ADC_CHANNELS * sizeof(uint16_t));
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
    sampling_hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV; 
    sampling_hadc.Init.LowPowerAutoWait = DISABLE;
    sampling_hadc.Init.LowPowerAutoPowerOff = DISABLE;
    sampling_hadc.Init.ContinuousConvMode = DISABLE;
    sampling_hadc.Init.NbrOfConversion = 1;
    sampling_hadc.Init.DiscontinuousConvMode = DISABLE;
    sampling_hadc.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T1_CC4;
    sampling_hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
    sampling_hadc.Init.DMAContinuousRequests = DISABLE;
    sampling_hadc.Init.Overrun = ADC_OVR_DATA_PRESERVED;
    sampling_hadc.Init.OversamplingMode = DISABLE;
    sampling_hadc.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_HIGH;
    error_handler_msg(HAL_ADC_Init(&sampling_hadc), "Failed to init ADC");

    // Init channels
    ADC_ChannelConfTypeDef channel_conf = {0};
    channel_conf.Rank = ADC_RANK_CHANNEL_NUMBER;
    channel_conf.SamplingTime = ADC_SAMPLETIME_79CYCLES_5;
    for (uint8_t i = 0; i < NO_ADC_CHANNELS; i++) {
        channel_conf.Channel = ADC_CHANNELS[i];
        channel_conf.Rank = ADC_RANKS[i];
        error_handler_msg(HAL_ADC_ConfigChannel(&sampling_hadc, &channel_conf), "Failed to init channel");
    }

    // Init DMA
    HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
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
    HAL_ADC_Start_DMA(&sampling_hadc, (uint32_t*)sample_buf, NO_ADC_CHANNELS * 2);
}

// When half the double buffer has filled mark it ready for reading
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
    if (hadc->Instance != ADC_INSTANCE)
        return;

    full_index ^= 1;
    buffer_ready = true;

    // uint16_t neutral =
}

extern void DMA_ISR(void) {
    HAL_DMA_IRQHandler(&sampling_hdma);
}