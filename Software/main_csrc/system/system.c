#include "system.h"
#include "stm32c0xx_hal.h"

// Get stuck in a while loop incase of an error
// This can easily be detected when debugging
void error_handler(HAL_StatusTypeDef status) {
  while (status != HAL_OK) (void) 0;
}

// Get stuck in a while loop incase of an error
// Also takes a pointer to an error message to be viewed by the debugger
void error_handler_msg(HAL_StatusTypeDef status, volatile char* msg) {
    while (status != HAL_OK) (void) msg;
}

// Init clocks so:
// SYSCLK       = 48 MHz
// HCLK         = 48 MHz
// APB clocks   = 48 MHz
void init_clocks(void) {
    // Oscillator config
    {
        // Turn on LSI and turn HSE, HSI, and LSE off
        RCC_OscInitTypeDef osc_cfg;
        osc_cfg.OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_LSE;
        osc_cfg.HSIState = RCC_HSI_OFF;
        osc_cfg.HSEState = RCC_HSE_OFF;
        osc_cfg.LSIState = RCC_LSI_ON;
        osc_cfg.LSEState = RCC_LSE_OFF;
        osc_cfg.HSIDiv = RCC_HSI_DIV1;
        error_handler_msg(HAL_RCC_OscConfig(&osc_cfg), "Error with oscillator config");
    }

    // Clock config
    {
        RCC_ClkInitTypeDef clk_cfg = {
            .ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1,
            .SYSCLKSource = RCC_SYSCLKSOURCE_LSI,
            .SYSCLKDivider = RCC_SYSCLK_DIV1,
            .AHBCLKDivider = RCC_HCLK_DIV1,
            .APB1CLKDivider = RCC_APB1_DIV1,
        };
        error_handler_msg(HAL_RCC_ClockConfig(&clk_cfg, FLASH_LATENCY_0), "Error with clock config");
    }

}

// Returns the tick frequency of the systick interrupt in Hz
uint32_t get_tick_frequency(void) {
    return 1000 / HAL_GetTickFreq();
}

extern void SysTick_Handler(void) {
    HAL_IncTick();
}