#ifndef MOTOR_H
#define MOTOR_H

#include "bdc_motor.h"
#include "esp_err.h"
#include <stdbool.h>

#define BDC_MCPWM_TIMER_RESOLUTION_HZ 10000000 // 10MHz, 1 tick = 0.1us
#define BDC_MCPWM_FREQ_HZ             25000    // 25KHz PWM
#define BDC_MCPWM_DUTY_TICK_MAX       (BDC_MCPWM_TIMER_RESOLUTION_HZ / BDC_MCPWM_FREQ_HZ) // maximum value we can set for the duty cycle, in ticks
typedef struct Motor_t* motor_handle_t;


motor_handle_t motor_create(uint8_t id_motor, uint8_t pwmPinA, uint8_t pwmPinB,
                            uint8_t mcpwm_group_id, uint32_t current_limit);

esp_err_t corre_forest(void *motor_handle, bool forward, uint32_t speed); // enciende el motor en la direccion dada y a la velocidad dada
esp_err_t apagalo_otto(void * motor_handle);// apaga el motor.
esp_err_t cuando_lo_apago(void * motor_handle,bool forward, uint32_t time_out_ms); // hace un encendido y apagado temporizado
#define encender_motor_simple(m,f) cuando_lo_apago(m, f, 0) //una trampita para agilizar codigo
#define encender_motor_tiempo(m,f,t) cuando_lo_apago(m, f, t)// lo mismo de arriba

esp_err_t cuanto_le_mide(void * motor_handle); // medimos la referencia de corriente en el adc correspondiente
esp_err_t vamos_a_probar(void * motor_handle); // un test rapido


#endif