#pragma once
// STUB di ping/ping_sock.h (stesse firme di ESP-IDF 5.5)
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "lwip/ip_addr.h"
#define IP_DEFAULT_TTL 64
#define ESP_TASK_PING_STACK 2048
typedef void* esp_ping_handle_t;
typedef struct {
  uint32_t count; uint32_t interval_ms; uint32_t timeout_ms; uint32_t data_size; int tos; int ttl;
  ip_addr_t target_addr; uint32_t task_stack_size; uint32_t task_prio; uint32_t interface;
} esp_ping_config_t;
#define ESP_PING_DEFAULT_CONFIG() { 5, 1000, 1000, 64, 0, IP_DEFAULT_TTL, {{{0}}, 0}, ESP_TASK_PING_STACK, 2, 0 }
typedef struct {
  void* cb_args;
  void (*on_ping_success)(esp_ping_handle_t hdl, void* args);
  void (*on_ping_timeout)(esp_ping_handle_t hdl, void* args);
  void (*on_ping_end)(esp_ping_handle_t hdl, void* args);
} esp_ping_callbacks_t;
typedef enum { ESP_PING_PROF_SEQNO, ESP_PING_PROF_TOS, ESP_PING_PROF_TTL, ESP_PING_PROF_REQUEST, ESP_PING_PROF_REPLY, ESP_PING_PROF_IPADDR,
               ESP_PING_PROF_SIZE, ESP_PING_PROF_TIMEGAP, ESP_PING_PROF_DURATION } esp_ping_profile_t;
esp_err_t esp_ping_new_session(const esp_ping_config_t* config, const esp_ping_callbacks_t* cbs, esp_ping_handle_t* hdl_out);
esp_err_t esp_ping_delete_session(esp_ping_handle_t hdl);
esp_err_t esp_ping_start(esp_ping_handle_t hdl);
esp_err_t esp_ping_stop(esp_ping_handle_t hdl);
esp_err_t esp_ping_get_profile(esp_ping_handle_t hdl, esp_ping_profile_t profile, void* data, uint32_t size);
