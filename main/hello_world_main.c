/**
 * @file hello_world_main.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2026-03-01
 *
 * @copyright Copyright (c) 2026
 *
 */

#include "freertos/FreeRTOS.h"

#include "MH_Z19C_8N1.h"
#include "Maquina_de_stados.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "hal/adc_types.h"
#include "motor.h"
#include "mqtt_client.h"
#include "mqtt_handle.h"
#include "portmacro.h"
#include "reading_rod.h"

#include <stdio.h>
#include <string.h>

#define MYSSID "Altice-98C0E0"
#define PASSWD "!0sirys!"
#define MQTT_COMMAND_TAG "MQTT_HEX"

// #################################################################################################
// ********************** declaracion de estructuras
// #################################################################################################
typedef struct {
  uint32_t pwmA;
  uint32_t pwmB;
} pwm_pins_t;

typedef struct {
  motor_handle_t motor;
  adc_channel_t canal;
} task_argumento_t;

// #################################################################################################
// ********************** declaracion de variables globales
// #################################################################################################

SemaphoreHandle_t sem_arm_hum_extender;
SemaphoreHandle_t sem_arm_hum_retraer;
SemaphoreHandle_t sem_arm_ph_extender;
SemaphoreHandle_t sem_arm_ph_retraer;
SemaphoreHandle_t sem_co2_active;
esp_mqtt_client_handle_t cliente;
uint8_t Fhum_task;
uint8_t Fph_task;
uint8_t Fco2_task;

eventos_t evento_actual;
int ph_value;
int hum_value;
uint16_t co2_value;
extern const uint8_t hivemq_ca_pem_start[] asm("_binary_hivemq_ca_pem_start");
// #################################################################################################
/********************* declaracion de prototipos*/
// #################################################################################################
void mqtt_connection(void *pvParameters);
void mqtt_commands(const char *topic, int topic_len, const char *data,
                   int data_len);
void error_handle(void *pvParameters);
void mqtt_publish(void *pvParameters);
void Task_ph_sensor(void *pvParameters);
void Task_hum_sensor(void *pvParameters);
void Task_CO2_sensor(void *pvParameters);

// ----------------------------------------------------------------------------------
/**APP MAIN*/
// ----------------------------------------------------------------------------------
/**
 * @brief
 *
 */
void app_main(void) {

  // #################################################################################################
  // ********************** declaracion y asignacion de las variables
  // #################################################################################################
  uint32_t current_limit = CURRENT_LIMIT(2.7); // limite de corriente general
  pwm_pins_t pines_pwm[3] = {{25, 26}, {27, 14}, {4, 13}}; // pinouts pwm

  adc_channel_t active_channels[6] = {ADC_CHANNEL_0, ADC_CHANNEL_1,
                                      ADC_CHANNEL_2, ADC_CHANNEL_3,
                                      ADC_CHANNEL_6, ADC_CHANNEL_7};

  adc_channel_t cs_current[3] = {ADC_CHANNEL_3, ADC_CHANNEL_6,
                                 ADC_CHANNEL_7}; // inputs de current sense

  motor_handle_t motor[3]; // motores

  static task_argumento_t args_ph;
  static task_argumento_t args_hum;

  esp_mqtt_client_config_t cfg = {
      .broker.address.uri =
          "mqtts://f1d6fee79c1f47448d6f46c663b50b64.s1.eu.hivemq.cloud:8883",
      .broker.verification.certificate = (const char *)hivemq_ca_pem_start,
      .credentials.authentication.password = "Rover01!",
      .credentials.username = "FnRover"

  };
  mh_z19c_init(UART_CO2, PIN_CO2_TX, PIN_CO2_RX);

  cliente = NULL;
  Fhum_task = 0;
  Fph_task = 0;
  Fco2_task = 0;
  evento_actual = STOP;

  /* ********************** creacion de todos los modulos necesarios */
  /*creacion de motores*/
  for (uint8_t i = 0; i < (sizeof(motor) / sizeof(motor[0])); i++) {
    motor[i] = motor_create(i, pines_pwm[i].pwmA, pines_pwm[i].pwmB, 1,
                            current_limit, cs_current[i]);
  }

  /*Creacion de semaforos*/
  sem_arm_hum_extender = xSemaphoreCreateBinary();
  sem_arm_hum_retraer = xSemaphoreCreateBinary();
  sem_arm_ph_extender = xSemaphoreCreateBinary();
  sem_arm_ph_retraer = xSemaphoreCreateBinary();
  sem_co2_active = xSemaphoreCreateBinary();
  // asignar valores
  args_ph.canal = ADC_CHANNEL_0;
  args_ph.motor = motor[0];
  args_hum.canal = ADC_CHANNEL_1;
  args_hum.motor = motor[1];
  wifi_init_sta(MYSSID, PASSWD);
  cliente = mqtt_app_start(&cfg, mqtt_commands);
  adc1_manager_init(active_channels, 6);
  /* tareas creadas, mqtt core 0 y las demas tareas donde este disponible*/
  xTaskCreatePinnedToCore(mqtt_publish, "MQTT_Interface", 8400, cliente, 5,
                          NULL, 0);
  xTaskCreatePinnedToCore(Task_ph_sensor, "Sensor_PH", 4096, &args_ph, 9, NULL,
                          tskNO_AFFINITY);

  xTaskCreatePinnedToCore(Task_hum_sensor, "Sensor_HUMEDAD", 4096, &args_hum, 9,
                          NULL, tskNO_AFFINITY);
  xTaskCreatePinnedToCore(Task_CO2_sensor, "Sensor_CO2", 4096, NULL, 7, NULL,
                          tskNO_AFFINITY);
}

/**
 * @brief
 *
 * @param pvParameters
 */
void mqtt_publish(void *pvParameters) {
  char payload[128];
  esp_mqtt_client_handle_t cliente = (esp_mqtt_client_handle_t)pvParameters;
  if (cliente != NULL) {

    esp_mqtt_client_publish(cliente, "rover/estado", "conectado", 0, 0, 0);
  }
  while (1) {
    if (cliente != NULL) {
      snprintf(payload, sizeof(payload), "{\"ph\":%d,\"hum\":%d,\"co2\":%d}",
               ph_value, hum_value, co2_value);
      esp_mqtt_client_publish(cliente, "rover/sensores", payload, 0, 0, 0);
    }
    // Aquí se implementaría la lógica para enviar mensajes MQTT
    // y actualizar el estado del sistema en consecuencia.
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}
/**
 * @brief
 *
 * @param topic
 * @param topic_len
 * @param data
 * @param data_len
 */

void mqtt_commands(const char *topic, int topic_len, const char *data,
                   int data_len) {
  if (strncmp(topic, "rover/comandos", topic_len) == 0) {
    if (data_len < 1)
      return;
    switch (data[0]) {
    case '1': // comando para ph extender brazo
      if (sem_arm_ph_extender != NULL) {
        xSemaphoreGive(sem_arm_ph_extender);
        ESP_LOGI(MQTT_COMMAND_TAG, "Comando 0x01, sensor de ph extendido");
        break;
      }
      ESP_LOGE(MQTT_COMMAND_TAG, "ERROR al crear el semaforo ph_extender");
      break;
    case '2': // comando para hum extender
      if (sem_arm_hum_extender != NULL) {
        xSemaphoreGive(sem_arm_hum_extender);

        ESP_LOGI(MQTT_COMMAND_TAG, "Comando 0x02, sensor de humedad extendido");
        break;
      }

      ESP_LOGE(MQTT_COMMAND_TAG, "ERROR al crear el semaforo hum_extender");
      break;

    case '3': // comando para ph retraer
      if (sem_arm_ph_retraer != NULL) {
        xSemaphoreGive(sem_arm_ph_retraer);

        ESP_LOGI(MQTT_COMMAND_TAG, "Comando 0x02, retrayendo sensor de ph");
        break;
      }

      ESP_LOGE(MQTT_COMMAND_TAG, "ERROR al crear el semaforo ph_retraer");
      break;
    case '4': // comando para hum retraer
      if (sem_arm_hum_retraer != NULL) {
        xSemaphoreGive(sem_arm_hum_retraer);

        ESP_LOGI(MQTT_COMMAND_TAG,
                 "Comando 0x02, retrayendo sensor de humedad");
        break;
      }

      ESP_LOGE(MQTT_COMMAND_TAG, "ERROR al crear el semaforo ph_retraer");
      break;
    case '5':
      if (sem_co2_active != NULL) {
        xSemaphoreGive(sem_co2_active);

        ESP_LOGI(MQTT_COMMAND_TAG, "Comando 0x05, Lectura de CO2");
        break;
      }

      ESP_LOGE(MQTT_COMMAND_TAG, "ERROR al crear el semaforo co2_active");
      break;
    default:
      ESP_LOGE(MQTT_COMMAND_TAG, "comando no reconocido 0x%02X", data[0]);
      break;
    }
  }
  /*Aca debo activar o desactivar los semaforos para correr las funciones*/
}

/*extiende los motores*/
static esp_err_t extender_motor(motor_handle_t motor) {
  corre_forest(motor, true, 84);
  vTaskDelay(pdMS_TO_TICKS(200));

  while (1) {
    uint32_t current = adc_manager_read_raw(get_current_channel(motor), 5);
    if (current >= get_current_limit(motor)) {
      apagalo_otto(motor);
      return ESP_OK;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
/*retraes los motores*/
static esp_err_t retraer_motor(motor_handle_t motor) {
  corre_forest(motor, false, 84);
  vTaskDelay(pdMS_TO_TICKS(200));

  while (1) {
    uint32_t current = adc_manager_read_raw(get_current_channel(motor), 5);
    if (current >= get_current_limit(motor)) {
      apagalo_otto(motor);
      return ESP_OK;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

/**
 * @brief
 *
 * @param pvParameters
 */
void Task_ph_sensor(void *pvParameters) {
  task_argumento_t *parametros = (task_argumento_t *)pvParameters;
  if (parametros->motor == NULL) {
    // Motor no definido
    apagalo_otto(parametros->motor);
  }
  while (1) {
    if (xSemaphoreTake(sem_arm_ph_extender, 0) == pdTRUE) {
      if (extender_motor(parametros->motor) == ESP_OK) {
        Fph_task = 1;
      }
    }
    if (xSemaphoreTake(sem_arm_ph_retraer, 0) == pdTRUE) {
      if (retraer_motor(parametros->motor) == ESP_OK) {
        Fph_task = 0;
      }
    }
    if (Fph_task == 1) {
      ph_value = adc_manager_read_raw(parametros->canal, 5);
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

/**
 * @brief
 *
 * @param pvParameters
 */
void Task_hum_sensor(void *pvParameters) {

  task_argumento_t *parametros = (task_argumento_t *)pvParameters;
  if (parametros->motor == NULL) {
    // Motor no definido
    apagalo_otto(parametros->motor);
  }
  while (1) {
    if (xSemaphoreTake(sem_arm_hum_extender, 0) == pdTRUE) {
      if (extender_motor(parametros->motor) == ESP_OK) {
        Fhum_task = 1;
      }
    }
    if (xSemaphoreTake(sem_arm_hum_retraer, 0) == pdTRUE) {
      if (retraer_motor(parametros->motor) == ESP_OK) {
        Fhum_task = 0;
      }
    }
    if (Fhum_task == 1) {

      hum_value = adc_manager_read_raw(parametros->canal, 5);
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

/**
 * @brief
 *
 * @param pvParameters
 */
void Task_CO2_sensor(void *pvParameters) {
  uint16_t temp_val = 0;

  mh_z19c_set_auto_calibration(UART_CO2, false);

  while (1) {
    if (xSemaphoreTake(sem_co2_active, portMAX_DELAY)) {

      temp_val = mh_z19c_read_co2(UART_CO2);
      if (temp_val >= 400 && temp_val <= 10000) {
        co2_value = temp_val;
      }
      vTaskDelay(pdMS_TO_TICKS(2000));
    }
  }
}