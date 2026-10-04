#pragma once
// STUB ricavato da esp-mqtt (ESP-IDF 5.5), solo i campi usati
#include <stdint.h>
#include "esp_err.h"
typedef const char* esp_event_base_t;
#define ESP_EVENT_ANY_ID (-1)
typedef struct esp_mqtt_client* esp_mqtt_client_handle_t;
typedef enum{MQTT_EVENT_ANY=-1,MQTT_EVENT_ERROR=0,MQTT_EVENT_CONNECTED,MQTT_EVENT_DISCONNECTED,MQTT_EVENT_SUBSCRIBED,MQTT_EVENT_UNSUBSCRIBED,MQTT_EVENT_PUBLISHED,MQTT_EVENT_DATA} esp_mqtt_event_id_t;
typedef enum{MQTT_ERROR_TYPE_NONE=0,MQTT_ERROR_TYPE_TCP_TRANSPORT,MQTT_ERROR_TYPE_CONNECTION_REFUSED} esp_mqtt_error_type_t;
typedef struct{esp_mqtt_error_type_t error_type;} esp_mqtt_error_codes_t;
typedef struct{esp_mqtt_event_id_t event_id; char*data; int data_len; int total_data_len; char*topic; int topic_len; esp_mqtt_error_codes_t*error_handle;} *esp_mqtt_event_handle_t;
typedef struct{
 struct{ struct{const char*uri;const char*hostname;uint32_t port;}address; struct{bool use_global_ca_store; esp_err_t (*crt_bundle_attach)(void*); const char*certificate; size_t certificate_len; bool skip_cert_common_name_check;}verification; }broker;
 struct{const char*username;const char*client_id;struct{const char*password;}authentication;}credentials;
 struct{struct{const char*topic;const char*msg;int msg_len;int qos;int retain;}last_will;int keepalive;}session;
 struct{int reconnect_timeout_ms;}network;} esp_mqtt_client_config_t;
typedef void (*esp_event_handler_t)(void*,esp_event_base_t,int32_t,void*);
esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t*); esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t,esp_mqtt_event_id_t,esp_event_handler_t,void*);
esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t); esp_err_t esp_mqtt_client_stop(esp_mqtt_client_handle_t); esp_err_t esp_mqtt_client_destroy(esp_mqtt_client_handle_t);
int esp_mqtt_client_publish(esp_mqtt_client_handle_t,const char*,const char*,int,int,int); int esp_mqtt_client_subscribe(esp_mqtt_client_handle_t,const char*,int);
