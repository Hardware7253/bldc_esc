// This module inits ADC channels reading

#pragma once

#include <stdbool.h>
#include <stdint.h>


void start_sampling(void);
uint16_t get_i_read(void);