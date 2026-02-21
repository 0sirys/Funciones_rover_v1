/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_log.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "motor.h"
#include <stdbool.h>

static const char *TAG = "example";

// Enable this config,  we will print debug formated string, which in return can
// be captured and parsed by Serial-Studio
#define SERIAL_STUDIO_DEBUG CONFIG_SERIAL_STUDIO_DEBUG

#define BDC_MCPWM_TIMER_RESOLUTION_HZ 10000000 // 10MHz, 1 tick = 0.1us
#define BDC_MCPWM_FREQ_HZ 25000                // 25KHz PWM
#define BDC_MCPWM_DUTY_TICK_MAX                                                \
  (BDC_MCPWM_TIMER_RESOLUTION_HZ /                                             \
   BDC_MCPWM_FREQ_HZ) // maximum value we can set for the duty cycle, in ticks

#define BDC_MCPWM_MOTOR1_R 21
#define BDC_MCPWM_MOTOR1_D 47

#define BDC_MCPWM_MOTOR2_R 48
#define BDC_MCPWM_MOTOR2_D 36

#define BDC_MCPWM_MOTOR3_R 37
#define BDC_MCPWM_MOTOR3_D 38

motor_handle_t motor[3];

void app_main(void) {

  ESP_LOGI(TAG, "Inicializando configuracion de motores");
  bdc_motor_config_t motor1_config = {
      .pwm_freq_hz = BDC_MCPWM_FREQ_HZ,
      .pwma_gpio_num = BDC_MCPWM_MOTOR1_R,
      .pwmb_gpio_num = BDC_MCPWM_MOTOR1_D,
  };

  bdc_motor_config_t motor2_config = {
      .pwm_freq_hz = BDC_MCPWM_FREQ_HZ,
      .pwma_gpio_num = BDC_MCPWM_MOTOR2_R,
      .pwmb_gpio_num = BDC_MCPWM_MOTOR2_D,
  };

  bdc_motor_config_t motor3_config = {
      .pwm_freq_hz = BDC_MCPWM_FREQ_HZ,
      .pwma_gpio_num = BDC_MCPWM_MOTOR3_R,
      .pwmb_gpio_num = BDC_MCPWM_MOTOR3_D,
  };

  // configuracion estandar  de periferico//

  bdc_motor_mcpwm_config_t mcpwm_config = {
      .group_id = 0,
      .resolution_hz = BDC_MCPWM_TIMER_RESOLUTION_HZ,
  };
  // Creacion de dispositivos BDC//
  ESP_ERROR_CHECK(bdc_motor_new_mcpwm_device(&motor1_config, &mcpwm_config,
                                             &motor1_ctx.motor));
  ESP_ERROR_CHECK(bdc_motor_new_mcpwm_device(&motor2_config, &mcpwm_config,
                                             &motor2_ctx.motor));
  ESP_ERROR_CHECK(bdc_motor_new_mcpwm_device(&motor3_config, &mcpwm_config,
                                             &motor3_ctx.motor));

  // Inicializacion de timers apagados
  init_motor_timer(&motor1_ctx);
  init_motor_timer(&motor2_ctx);
  init_motor_timer(&motor3_ctx);

  ESP_ERROR_CHECK(bdc_motor_enable(motor1_ctx.motor));
  ESP_LOGI(TAG, "Enable motor1");
  ESP_ERROR_CHECK(bdc_motor_enable(motor2_ctx.motor));
  ESP_LOGI(TAG, "Enable motor2");
  ESP_ERROR_CHECK(bdc_motor_enable(motor3_ctx.motor));
  ESP_LOGI(TAG, "Enable motor3");
  ESP_LOGI(TAG, "Sistema listo. Iniciando prueba asincrona...");

  //----------------------------------------------------
  // TESTE
  //----------------------------------------------------
  ESP_LOGI(TAG, "TEST CORRECTO...");
}
