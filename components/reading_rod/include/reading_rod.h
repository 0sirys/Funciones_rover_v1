// reading_rod.h
#ifndef READING_ROD_H
#define READING_ROD_H


#define V_REF 3.3f
#define ADC_MAX 4095.0f
#define VOLTAGE(raw) (((float)raw * V_REF) / ADC_MAX)

#include "hal/adc_types.h"

void adc1_manager_init(adc_channel_t *channels, uint8_t num_channels);
int adc_manager_read_raw(adc_channel_t channel, uint32_t timeout_ms);
float adc_manager_read_voltage(adc_channel_t channel, uint32_t timeout_ms);

#endif
