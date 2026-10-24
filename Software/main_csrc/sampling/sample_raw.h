// This module inits ADC channels reading

#pragma once

#include <stdbool.h>
#include <stdint.h>


bool is_buffer_ready(void);
uint16_t* get_raw_buf(void);
void start_sampling(void);