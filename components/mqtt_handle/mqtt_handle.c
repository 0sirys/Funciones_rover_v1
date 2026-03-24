/**
 * @file mqtt_handle.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2026-03-01
 *
 * @copyright Copyright (c) 2026
 *
 */
#include "mqtt_handle.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_event_base.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_wifi_types_generic.h"
#include "freertos/idf_additions.h"
#include "mqtt_client.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "MQTT_ROVER";
static bool mqtt_conectado = false;
static mqtt_receive_cb_t app_receive_cb = NULL;
static int retry_num = 0;
static EventGroupHandle_t wifi_event_group;

/**
 * @brief
 *
 * @param handler_args
 * @param base
 * @param event_id
 * @param event_data
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data) {
  esp_mqtt_event_handle_t event = event_data;
  esp_mqtt_client_handle_t client = event->client;
  switch ((esp_mqtt_event_id_t)event_id) {
  case MQTT_EVENT_CONNECTED:
    ESP_LOGI(TAG, "¡FUNCIONES Conctadas al BROKER MQTT!");
    mqtt_conectado = true;

    esp_mqtt_client_subscribe(client, "rover/comandos", 0);

    break;
  case MQTT_EVENT_DISCONNECTED:
    ESP_LOGI(TAG, "!FUNCIONES desconectadas del BROKER MQTT!");
    mqtt_conectado = false;
    break;

  case MQTT_EVENT_DATA:
    ESP_LOGI(TAG, "MQTT_EVENT_DATA");
    if (app_receive_cb != NULL) {
      app_receive_cb(event->topic, event->topic_len, event->data,
                     event->data_len);
    }
    break;
  case MQTT_EVENT_ERROR:
    ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
    if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
      ESP_LOGI(TAG, "Last error code reported from esp-tls: 0x%x",
               event->error_handle->esp_tls_last_esp_err);
      ESP_LOGI(TAG, "Last tls stack error number: 0x%x",
               event->error_handle->esp_tls_stack_err);
      ESP_LOGI(TAG, "Last captured errno : %d",
               event->error_handle->esp_transport_sock_errno);
    } else if (event->error_handle->error_type ==
               MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
      ESP_LOGI(TAG, "Connection refused error: 0x%x",
               event->error_handle->connect_return_code);
    } else {
      ESP_LOGW(TAG, "Unknown error type: 0x%x",
               event->error_handle->error_type);
    }
    break;
  default:
    ESP_LOGI(TAG, "Other event id:%d", event->event_id);
    break;
  }
}

/**
 * @brief
 *
 * @param cfg
 * @param on_receive_callback
 * @return esp_mqtt_client_handle_t
 */
esp_mqtt_client_handle_t mqtt_app_start(esp_mqtt_client_config_t *cfg,
                                        mqtt_receive_cb_t on_receive_callback) {
  app_receive_cb = on_receive_callback;

  ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes",
           esp_get_free_heap_size());
  // inicializo cliente
  esp_mqtt_client_handle_t client = esp_mqtt_client_init(cfg);
  /*Registramos los eventos*/
  esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler,
                                 NULL);
  /*Arranco el clietne*/
  esp_mqtt_client_start(client);
  return client;
}

/**
 * @brief
 *
 * @param my_ssid
 * @param my_password
 */
void wifi_init_sta(const char *my_ssid, const char *my_password) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  wifi_event_group = xEventGroupCreate();
  ESP_ERROR_CHECK(esp_netif_init());

  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  // registro de eventos
  esp_event_handler_instance_t instance_any_id;
  esp_event_handler_instance_t instance_got_ip;
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL,
      &instance_any_id));
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL,
      &instance_got_ip));

  wifi_config_t wifi_cfg = {
      .sta =
          {
              .threshold.authmode = WIFI_AUTH_WPA2_PSK,
          },
  };
  snprintf((char *)wifi_cfg.sta.ssid, sizeof(wifi_cfg.sta.ssid), "%s", my_ssid);
  snprintf((char *)wifi_cfg.sta.password, sizeof(wifi_cfg.sta.password), "%s",
           my_password);

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
  ESP_ERROR_CHECK(esp_wifi_start());

  // 3. BLOQUEAMOS HASTA TENER INTERNET O FALLAR (El Cerrojo)
  EventBits_t bits =
      xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                          pdFALSE, pdFALSE, portMAX_DELAY);

  if (bits & WIFI_CONNECTED_BIT) {
    ESP_LOGI(TAG, "Conexión Wi-Fi exitosa.");
  } else if (bits & WIFI_FAIL_BIT) {
    ESP_LOGE(TAG, "Error fatal de Wi-Fi.");
  }
}

/**
 * @brief
 *
 * @param arg
 * @param event_base
 * @param event_id
 * @param event_data
 */
void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();

  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    if (retry_num < MAX_RETRYS) {
      esp_wifi_connect();
      retry_num++;
      ESP_LOGW(TAG, "Reintentando conectar al router...");
    } else {
      xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
    }
    ESP_LOGE(TAG, "Fallo al conectar al router");

  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "¡Conectado! IP asignada: " IPSTR,
             IP2STR(&event->ip_info.ip));
    retry_num = 0;
    xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
  }
}
