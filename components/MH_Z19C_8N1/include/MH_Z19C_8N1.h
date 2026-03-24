#ifndef MH_Z19C_8N1_H
#define MH_Z19C_8N1_H

#include <freertos/FreeRTOS.h>

#include "freertos/projdefs.h"
#include "hal/uart_types.h"
#include "soc/clk_tree_defs.h"
#include <driver/gpio.h>
#include <driver/uart.h>
#include <freertos/idf_additions.h>
#include <freertos/task.h>
#include <stdbool.h>
#include <stdint.h>
//********Puedes cambiar estos parametros, no muevas nada mas hdp */
#define UART_CO2 UART_NUM_1
#define PIN_CO2_TX 32
#define PIN_CO2_RX 33

//************************** */
#define BYTE0 0xFF
#define BYTE1 0x01
#define BYTE_FILLING 0x00
#define SELF_CALIBRATION 0x79
#define READ_COMMAND {BYTE0, BYTE1, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79}
#define CALIBRATE_MANUAL_COMMAND                                               \
  {BYTE0, BYTE1, 0x79, 0x00, 0x00, 0x00, 0x00, 0x00, 0x86}
#define CALIBRATE_AUTO_COMMAND                                                 \
  {BYTE0, BYTE1, 0x79, 0xA0, 0x00, 0x00, 0x00, 0x00, 0xE6}

typedef enum {
  WAIT_START_BYTE,
  WAIT_COMMAND_BYTE,
  READ_DATA,
  WAIT_CHECKSUM,

} mhz19_states;

uint16_t licuadora(uint8_t byte_rx);

void mh_z19c_init(uart_port_t uart_num, int tx_pin, int rx_pin);
uint16_t mh_z19c_read_co2(uart_port_t uart_num);
void mh_z19c_set_auto_calibration(uart_port_t uart_num, bool enable);

#endif