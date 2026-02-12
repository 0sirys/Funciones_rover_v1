#include "stm32_configure.h"
#include "esp_log.h"

static const char *TAG = "STM32_DRIVER";
static system_pin_config_t current_conf;

esp_err_t stm32_init(const system_pin_config_t *config) {
    if (config == NULL) return ESP_ERR_INVALID_ARG;
    current_conf = *config;

    ESP_LOGI(TAG, "Configurando Pines Independientes...");

    // 1. I2C Config (Igual que siempre)
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = config->i2c_bus.sda_io_num,
        .scl_io_num = config->i2c_bus.scl_io_num,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = config->i2c_bus.freq_hz,
        .clk_flags = 0,
    };
    i2c_param_config(config->i2c_bus.port_num, &conf);
    i2c_driver_install(config->i2c_bus.port_num, conf.mode, 0, 0, 0);

    // 2. CONFIGURAR SALIDAS (Pines 15, 16, 9)
    uint64_t output_mask = (1ULL << config->task_signals.task1_pin) | 
                           (1ULL << config->task_signals.task2_pin) | 
                           (1ULL << config->task_signals.task3_pin);

    gpio_config_t io_conf = {
        .pin_bit_mask = output_mask,
        .mode = GPIO_MODE_OUTPUT,   
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    
    // Iniciar todos en LOW (0V)
    gpio_set_level(config->task_signals.task1_pin, 0);
    gpio_set_level(config->task_signals.task2_pin, 0);
    gpio_set_level(config->task_signals.task3_pin, 0);

    ESP_LOGI(TAG, "Salidas configuradas en GPIOs %d, %d, %d", 
             config->task_signals.task1_pin, 
             config->task_signals.task2_pin, 
             config->task_signals.task3_pin);
             
    return ESP_OK;
}

esp_err_t stm32_send_data_i2c(uint8_t *data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (STM32_SLAVE_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(current_conf.i2c_bus.port_num, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

void stm32_set_task_done(int task_id, bool done) {
    int level = done ? 1 : 0;
    
    switch (task_id) {
        case 1:
            gpio_set_level(current_conf.task_signals.task1_pin, level);
            ESP_LOGI(TAG, "Tarea 1 (Pin %d): %s", current_conf.task_signals.task1_pin, done ? "HIGH" : "LOW");
            break;
        case 2:
            gpio_set_level(current_conf.task_signals.task2_pin, level);
            ESP_LOGI(TAG, "Tarea 2 (Pin %d): %s", current_conf.task_signals.task2_pin, done ? "HIGH" : "LOW");
            break;
        case 3:
            gpio_set_level(current_conf.task_signals.task3_pin, level);
            ESP_LOGI(TAG, "Tarea 3 (Pin %d): %s", current_conf.task_signals.task3_pin, done ? "HIGH" : "LOW");
            break;
        default:
            ESP_LOGW(TAG, "ID de Tarea desconocido: %d", task_id);
            break;
    }
}