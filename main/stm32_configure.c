#include "stm32_configure.h"
#include "esp_log.h"

static const char *TAG = "STM32_DRIVER";

// Guardamos una copia local de la configuración para usarla en las funciones de envío/lectura
static stm32_config_t drv_config;

esp_err_t stm32_init(const stm32_config_t *config) {
    if (config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 1. Guardar la configuración en la variable local
    drv_config = *config;

    ESP_LOGI(TAG, "Iniciando Driver STM32 en I2C Port %d...", drv_config.i2c_port);

    // 2. Configurar I2C Maestro
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = config->pin_sda,
        .scl_io_num = config->pin_scl,
        .sda_pullup_en = GPIO_PULLUP_DISABLE, // Resistencias externas
        .scl_pullup_en = GPIO_PULLUP_DISABLE, // Resistencias externas
        .master.clk_speed = config->i2c_freq_hz,
        .clk_flags = 0,
    };

    esp_err_t err = i2c_param_config(config->i2c_port, &conf);
    if (err != ESP_OK) return err;

    err = i2c_driver_install(config->i2c_port, conf.mode, 0, 0, 0);
    if (err != ESP_OK) return err;

    // 3. Configurar Pines ACT (Entradas)
    // Creamos una máscara de bits con los 3 pines
    uint64_t pin_mask = (1ULL << config->pin_act1) | 
                        (1ULL << config->pin_act2) | 
                        (1ULL << config->pin_act3);

    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // 4. Configurar Pines D0/D1 (Entradas/Salidas según necesites)
    if (config->pin_d0 >= 0 && config->pin_d1 >= 0) {
        gpio_config_t data_conf = {
            .pin_bit_mask = (1ULL << config->pin_d0) | (1ULL << config->pin_d1),
            .mode = GPIO_MODE_INPUT, // Asumimos entrada por defecto
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE
        };
        gpio_config(&data_conf);
    }

    ESP_LOGI(TAG, "Driver STM32 inicializado correctamente.");
    return ESP_OK;
}

esp_err_t stm32_send_data(const uint8_t *data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    // Byte de dirección + Bit de Escritura (0)
    i2c_master_write_byte(cmd, (STM32_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);

    // Ejecutar comando con timeout de 1 segundo
    esp_err_t ret = i2c_master_cmd_begin(drv_config.i2c_port, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error enviando datos I2C: %s", esp_err_to_name(ret));
    }
    return ret;
}

uint8_t stm32_read_status(void) {
    // Leemos los pines usando la configuración guardada
    int b1 = gpio_get_level(drv_config.pin_act1);
    int b2 = gpio_get_level(drv_config.pin_act2);
    int b3 = gpio_get_level(drv_config.pin_act3);
    
    // Retornamos valor combinado: (ACT3 << 2) | (ACT2 << 1) | ACT1
    return (b3 << 2) | (b2 << 1) | b1;
}