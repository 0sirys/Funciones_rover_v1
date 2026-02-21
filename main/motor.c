#include "motor.h"
#include "esp_timer.h"

struct Motor_t {

  uint8_t id_motor;         // Identificador del motor
  bdc_motor_handle_t motor; // handle del driver bdc
  uint32_t current;         // ultima lectura de corriente
  uint32_t current_limit;   // limite de corriente
  bool timer_active;        // estado del contado
  bool is_running;          // estado del motor
  bool forward;             // direccion del motor
};

motor_handle_t motor_create(uint8_t id_motor, uint8_t pwmPinA, uint8_t pwmPinB,
                            uint8_t mcpwm_group_id, uint32_t current_limit) {
  motor_handle_t motor_handle = malloc(sizeof(struct Motor_t));
  if (motor_handle == NULL) {
    return NULL;
  }
  bdc_motor_config_t motor_config = {
      .pwm_freq_hz = BDC_MCPWM_FREQ_HZ,
      .pwma_gpio_num = pwmPinA,
      .pwmb_gpio_num = pwmPinB,
  };
  bdc_motor_mcpwm_config_t mcpwm_config = {
      .group_id = mcpwm_group_id,
      .resolution_hz = BDC_MCPWM_TIMER_RESOLUTION_HZ,
  };
  ESP_ERROR_CHECK(bdc_motor_new_mcpwm_device(&motor_config, &mcpwm_config,
                                             &motor_handle->motor));
  motor_handle->id_motor = id_motor;
  motor_handle->current = 0;
  motor_handle->current_limit = current_limit;
  motor_handle->timer_active = false;
  motor_handle->is_running = false;
  motor_handle->forward = false;
  ESP_ERROR_CHECK(bdc_motor_enable(motor_handle->motor));
  return motor_handle;
}

esp_err_t corre_forest(void *motor_handle, bool forward, uint32_t speed) {
  motor_handle_t motor = (motor_handle_t)motor_handle;
  if (motor == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  bdc_motor_set_speed(motor->motor, speed);
  if (forward) {
    ESP_ERROR_CHECK(bdc_motor_forward(motor->motor));

    motor->forward = true;
  } else {
    ESP_ERROR_CHECK(bdc_motor_reverse(motor->motor));
    motor->forward = false;
  }
  motor->is_running = true;
  return ESP_OK;
}

esp_err_t apagalo_otto(void *motor_handle) {
  motor_handle_t motor = (motor_handle_t)motor_handle;
  if (motor == NULL) {
    return ESP_ERR_INVALID_ARG;
  }
  ESP_ERROR_CHECK(bdc_motor_brake(motor->motor));
  motor->is_running = false;
  return ESP_OK;
}
esp_err_t cuando_lo_apago(void *motor_handle, bool forward,
                          uint32_t time_out_ms);