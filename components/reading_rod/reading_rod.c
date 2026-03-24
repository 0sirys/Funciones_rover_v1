/**
 * @file reading_rod.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2026-03-01
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "freertos/FreeRTOS.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "freertos/idf_additions.h"
#include "freertos/semphr.h"
#include "reading_rod.h"

static adc_oneshot_unit_handle_t adc_handle;
static SemaphoreHandle_t adc_mutex;

/**
 * @brief
 *
 * @param channels
 * @param num_channels
 */
void adc1_manager_init(adc_channel_t *channels, uint8_t num_channels) {
  adc_mutex = xSemaphoreCreateMutex();
  adc_oneshot_unit_init_cfg_t init_config = {
      .unit_id = ADC_UNIT_1,
      .ulp_mode = ADC_ULP_MODE_DISABLE,
  };
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));
  adc_oneshot_chan_cfg_t config = {
      .bitwidth = ADC_BITWIDTH_DEFAULT,
      .atten = ADC_ATTEN_DB_12,
  };
  for (int i = 0; i < num_channels; i++) {
    ESP_ERROR_CHECK(
        adc_oneshot_config_channel(adc_handle, channels[i], &config));
  }
}

/**
 * @brief
 *
 * @param channel
 * @param timeout_ms
 * @return int
 */
int adc_manager_read_raw(adc_channel_t channel, uint32_t timeout_ms) {
  int raw_result = -1;
  if (xSemaphoreTake(adc_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
    esp_err_t ret = adc_oneshot_read(adc_handle, channel, &raw_result);
    xSemaphoreGive(adc_mutex);
    return ret != ESP_OK ? -2 : raw_result;
  }
  return raw_result;
}

/**
 * @brief
 *
 * @param channel
 * @param timeout_ms
 * @return float
 */
float adc_manager_read_voltage(adc_channel_t channel, uint32_t timeout_ms) {

  int raw_result = 0;
  if (xSemaphoreTake(adc_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {
    esp_err_t ret = adc_oneshot_read(adc_handle, channel, &raw_result);
    xSemaphoreGive(adc_mutex);
    return ret != ESP_OK
               ? (float)-2.0f
               : (float)VOLTAGE(raw_result); // retorna -2 por error en hardware
  }

  return -1.0f; // mutex ocupado
}