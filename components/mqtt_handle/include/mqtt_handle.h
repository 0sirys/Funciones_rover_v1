// mqtt_handle.h
#ifndef MQTT_HANDLE_H
#define MQTT_HANDLE_H
#define MAX_RETRYS 5
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

#include "esp_event_base.h"
#include "mqtt_client.h"
#include <stddef.h>
#include <stdint.h>

/*defino un callback para cuando lleguen los datos*/

typedef void (*mqtt_receive_cb_t)(const char *topic, int topic_len,
                                  const char *data, int data_len);
/*me devuelve el cliente configurado*/
esp_mqtt_client_handle_t mqtt_app_start(esp_mqtt_client_config_t *cfg,
                                        mqtt_receive_cb_t client);

void wifi_init_sta(const char *ssid, const char *password);
void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data);
#endif
