#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "comunication_pins.h"
#include "stm32_configure.h"

static const char *TAG = "MAIN_APP";
 


// codigo de prueba para ver que el sp32 haga lo que tiene que hacer






void app_main(void) {
    // 1. Cargar Configuración
    system_pin_config_t my_config = DEFAULT_HARDWARE_CONFIG();

    // 2. Inicializar
    stm32_init(&my_config);

    while (1) {
        // --- SIMULACIÓN DE TAREAS ---

        // Tarea 1 empieza... y termina rápido
        ESP_LOGI(TAG, "Ejecutando Tarea 1...");
        vTaskDelay(pdMS_TO_TICKS(1000)); 
        stm32_set_task_done(1, true); // Pin 15 -> HIGH
        
        // Tarea 2 empieza... y toma más tiempo
        ESP_LOGI(TAG, "Ejecutando Tarea 2...");
        vTaskDelay(pdMS_TO_TICKS(1500)); 
        stm32_set_task_done(2, true); // Pin 16 -> HIGH

        // Tarea 3 empieza...
        ESP_LOGI(TAG, "Ejecutando Tarea 3...");
        vTaskDelay(pdMS_TO_TICKS(500));
        stm32_set_task_done(3, true); // Pin 9 -> HIGH

        // --- TODAS TERMINARON ---
        ESP_LOGW(TAG, "Todas las tareas completadas. Esperando reset...");
        vTaskDelay(pdMS_TO_TICKS(2000));

        // Reiniciar pines a LOW para el siguiente ciclo
        stm32_set_task_done(1, false);
        stm32_set_task_done(2, false);
        stm32_set_task_done(3, false);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}