#ifndef COMUNICATION_PINS_H
#define COMUNICATION_PINS_H

#include "driver/gpio.h"
#include "driver/i2c.h"

// ==========================================
// 1. MAPEO FÍSICO DE PINES
// ==========================================

// Comunicación I2C
#define PIN_I2C_SDA       3     // GPIO 3
#define PIN_I2C_SCL       35    // GPIO 35 (Original)

// Pines de Aviso de Tareas (Salidas Independientes) Estas van para el STM32
#define PIN_TASK_1_DONE   15    // GPIO 15 (Aviso Tarea 1)
#define PIN_TASK_2_DONE   16    // GPIO 16 (Aviso Tarea 2)
#define PIN_TASK_3_DONE   9     // GPIO 9  (Aviso Tarea 3)

// Datos Extra
#define PIN_DATA_D1       5     // GPIO 5

// ==========================================
// 2. ESTRUCTURA DE CONFIGURACIÓN
// ==========================================

typedef struct {
    // Configuración I2C
    struct {
        int sda_io_num;
        int scl_io_num;
        i2c_port_t port_num;
        uint32_t freq_hz;
    } i2c_bus;

    // Configuración de Señales de Tarea (Salidas)
    struct {
        int task1_pin;
        int task2_pin;
        int task3_pin;
    } task_signals;

} system_pin_config_t;

// Macro de Inicialización
#define DEFAULT_HARDWARE_CONFIG() { \
    .i2c_bus = { \
        .sda_io_num = PIN_I2C_SDA, \
        .scl_io_num = PIN_I2C_SCL, \
        .port_num = I2C_NUM_0, \
        .freq_hz = 400000 \
    }, \
    .task_signals = { \
        .task1_pin = PIN_TASK_1_DONE, \
        .task2_pin = PIN_TASK_2_DONE, \
        .task3_pin = PIN_TASK_3_DONE \
    } \
}

#endif // COMUNICATION_PINS_H