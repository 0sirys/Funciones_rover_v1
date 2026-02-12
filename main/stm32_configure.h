#ifndef STM32_CONFIGURE_H
#define STM32_CONFIGURE_H

#include "esp_err.h"
#include "comunication_pins.h"

#define STM32_SLAVE_ADDR  0x28 

/**
 * @brief Inicializa I2C y configura los 3 pines de tarea como SALIDAS.
 */
esp_err_t stm32_init(const system_pin_config_t *config);

/**
 * @brief Envía datos por I2C.
 */
esp_err_t stm32_send_data_i2c(uint8_t *data, size_t len);

/**
 * @brief Activa o desactiva la señal de "Terminado" para una tarea específica.
 * * @param task_id  El número de la tarea (1, 2 o 3).
 * @param done     true para poner HIGH (3.3V), false para LOW (0V).
 */
void stm32_set_task_done(int task_id, bool done);

#endif // STM32_CONFIGURE_H