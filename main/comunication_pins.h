#ifndef COMUNICATION_PINS_H
#define COMUNICATION_PINS_H

#include "driver/gpio.h"
#include "driver/i2c.h"

// ==========================================
// 1. PINES I2C_Comunication (Sensor)
// ==========================================
// Según tu imagen: I2C1
#define PIN_I2C1_D0       4     // GPIO 4 (Probablemente SDA)
#define PIN_I2C1_D1       5     // GPIO 5 (Probablemente SCL)

// ==========================================
// 2. PINES I2C_1 (STM32 Principal)
// ==========================================
// Según tu imagen: I2C_SCL_1 y I2C_SDA_1
#define PIN_I2C_SDA_1     3     // GPIO 3
#define PIN_I2C_SCL_1     35    // GPIO 35

// ==========================================
// 3. PINES STM-COMMUNICATION (Señales FN)
// ==========================================
// Según tu imagen: ACT_1, ACT_2, ACT_3
#define PIN_ACT_1         15    // GPIO 15 (FN1)
#define PIN_ACT_2         16    // GPIO 16 (FN2)
#define PIN_ACT_3         9     // GPIO 9  (FN3)

// ==========================================
// 4. ESTRUCTURA DE CONFIGURACIÓN
// ==========================================

typedef struct {
    // Grupo I2C_1 (Para hablar con STM32)
    struct {
        int sda;
        int scl;
        i2c_port_t port; 
        uint32_t freq;
    } stm_i2c;

    // Grupo I2C1 (Para leer el Sensor)
    struct {
        int d0; // SDA
        int d1; // SCL
        i2c_port_t port;
        uint32_t freq;
    } sensor_i2c;

    // Grupo ACT (Señales FN)
    struct {
        int fn1;
        int fn2;
        int fn3;
    } act_signals;

} system_config_t;

#define DEFAULT_CONFIG() { \
    .stm_i2c = { \
        .sda = PIN_I2C_SDA_1, .scl = PIN_I2C_SCL_1, \
        .port = I2C_NUM_0, .freq = 400000 \
    }, \
    .sensor_i2c = { \
        .d0 = PIN_I2C1_D0, .d1 = PIN_I2C1_D1, \
        .port = I2C_NUM_1, .freq = 100000 \
    }, \
    .act_signals = { \
        .fn1 = PIN_ACT_1, .fn2 = PIN_ACT_2, .fn3 = PIN_ACT_3 \
    } \
}

#endif