/**
 * @file MH_Z19C_8N1.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2026-03-01
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "MH_Z19C_8N1.h"
#include "esp_log.h"
#include "hal/uart_types.h"

uint8_t checkersum(uint8_t *buffer);
const char *TAG = "MH_Z19C";

/**
 * @brief
 *
 * @param uart_num
 * @param tx_pin
 * @param rx_pin
 */
void mh_z19c_init(uart_port_t uart_num, int tx_pin, int rx_pin) {
  esp_err_t err;
  const uart_config_t uart_config = {
      .baud_rate = 9600,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .source_clk = UART_SCLK_APB,
  };
  err = uart_driver_install(uart_num, 256, 0, 0, NULL, 0);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error al instalar el driver UART: %s", esp_err_to_name(err));
  }
  err = uart_param_config(uart_num, &uart_config);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error al configurar los paratros UART: %s",
             esp_err_to_name(err));
  }
  err = uart_set_pin(uart_num, tx_pin, rx_pin, UART_PIN_NO_CHANGE,
                     UART_PIN_NO_CHANGE);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Error al configurar los pines del  UART: %s",
             esp_err_to_name(err));
  }
}

/**
 * @brief
 *
 * @param uart_num
 * @return uint16_t
 */
uint16_t mh_z19c_read_co2(uart_port_t uart_num) {
  const uint8_t read_command[9] = READ_COMMAND;
  uint8_t rx_byte;
  uint16_t lectura = 0;
  uart_write_bytes(uart_num, read_command, 9);
  TickType_t tiempo_inicio = xTaskGetTickCount();
  while ((xTaskGetTickCount() - tiempo_inicio) < pdMS_TO_TICKS(100)) {
    int length = uart_read_bytes(uart_num, &rx_byte, 1, pdMS_TO_TICKS(10));
    if (length) {
      lectura = licuadora(rx_byte);
      if (lectura > 0) {
        return lectura;
      }
    }
  }
  return 0;
}

/**
 * @brief
 *
 * @param uart_num
 * @param enable
 */
void mh_z19c_set_auto_calibration(uart_port_t uart_num, bool enable) {
  const uint8_t cmd_auto_on[9] = CALIBRATE_AUTO_COMMAND;
  const uint8_t cmd_auto_off[9] = CALIBRATE_MANUAL_COMMAND;
  if (enable) {
    uart_write_bytes(uart_num, cmd_auto_on, 9);
    return;
  }
  uart_write_bytes(uart_num, cmd_auto_off, 9);
}

/**
 * @brief
 *
 * @param byte_rx
 * @return uint16_t
 */
uint16_t licuadora(uint8_t byte_rx) {
  static uint8_t read_buffer[9];
  static mhz19_states estado = WAIT_START_BYTE;
  static uint8_t indice = 0;
  switch (estado) {

  case WAIT_START_BYTE:
    if (byte_rx == 0xFF) {
      read_buffer[0] = byte_rx;
      estado = WAIT_COMMAND_BYTE;
    }

    break;
  case WAIT_COMMAND_BYTE:
    if (byte_rx == 0x86) {
      read_buffer[1] = byte_rx;
      indice = 2;
      estado = READ_DATA;
    } else {

      estado = WAIT_START_BYTE;
    }

    break;
  case READ_DATA:
    read_buffer[indice++] = byte_rx;
    if (indice == 8) {
      estado = WAIT_CHECKSUM;
    }
    break;
  case WAIT_CHECKSUM:
    read_buffer[8] = byte_rx;
    estado = WAIT_START_BYTE;
    if (checkersum(read_buffer) == 1) {
      return (read_buffer[2] << 8) | read_buffer[3];
    }

    break;

  default:
    estado = WAIT_START_BYTE;
    break;
  }
  return 0;
}

/**
 * @brief
 *
 * @param buffer
 * @return uint8_t
 */
uint8_t checkersum(uint8_t *buffer) {
  uint8_t checksum = 0;
  for (uint8_t i = 1; i < 8; i++) {
    checksum += buffer[i];
  }
  checksum = 0xff - checksum;
  checksum += 1;
  if (checksum != buffer[8]) {
    return 0;
  }
  return 1;
}
