#pragma once
// STUB minimo di esp_http_server.h (tipi usati dagli header di PsychicHttp)
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#ifdef __cplusplus
extern "C" {
#endif
enum http_method { HTTP_DELETE=0, HTTP_GET=1, HTTP_HEAD=2, HTTP_POST=3, HTTP_PUT=4, HTTP_OPTIONS=6 };
typedef void* httpd_handle_t;
typedef struct httpd_req { httpd_handle_t handle; int method; const char uri[513]; size_t content_len; void* aux; void* user_ctx; void* sess_ctx; void (*free_ctx)(void*); bool ignore_sess_ctx_changes; } httpd_req_t;
typedef bool (*httpd_uri_match_func_t)(const char* reference_uri, const char* uri_to_match, size_t match_upto);
typedef struct httpd_config { unsigned task_priority; size_t stack_size; int core_id; uint16_t server_port; uint16_t ctrl_port; uint16_t max_open_sockets; uint16_t max_uri_handlers; uint16_t max_resp_headers; uint16_t backlog_conn; bool lru_purge_enable; uint16_t recv_wait_timeout; uint16_t send_wait_timeout; void* global_user_ctx; void (*global_user_ctx_free_fn)(void*); void* global_transport_ctx; void (*global_transport_ctx_free_fn)(void*); bool enable_so_linger; int linger_timeout; bool keep_alive_enable; int keep_alive_idle; int keep_alive_interval; int keep_alive_count; esp_err_t (*open_fn)(httpd_handle_t,int); void (*close_fn)(httpd_handle_t,int); httpd_uri_match_func_t uri_match_fn; } httpd_config_t;
typedef enum { HTTPD_500_INTERNAL_SERVER_ERROR=0, HTTPD_501_METHOD_NOT_IMPLEMENTED, HTTPD_505_VERSION_NOT_SUPPORTED, HTTPD_400_BAD_REQUEST, HTTPD_401_UNAUTHORIZED, HTTPD_403_FORBIDDEN, HTTPD_404_NOT_FOUND, HTTPD_405_METHOD_NOT_ALLOWED, HTTPD_408_REQ_TIMEOUT, HTTPD_411_LENGTH_REQUIRED, HTTPD_414_URI_TOO_LONG, HTTPD_431_REQ_HDR_FIELDS_TOO_LARGE, HTTPD_ERR_CODE_MAX } httpd_err_code_t;
typedef struct httpd_uri { const char* uri; int method; esp_err_t (*handler)(httpd_req_t* r); void* user_ctx; bool is_websocket; bool handle_ws_control_frames; const char* supported_subprotocol; } httpd_uri_t;
typedef enum { HTTPD_WS_TYPE_CONTINUE=0, HTTPD_WS_TYPE_TEXT=1, HTTPD_WS_TYPE_BINARY=2, HTTPD_WS_TYPE_CLOSE=8, HTTPD_WS_TYPE_PING=9, HTTPD_WS_TYPE_PONG=10 } httpd_ws_type_t;
typedef struct httpd_ws_frame { bool final; bool fragmented; httpd_ws_type_t type; uint8_t* payload; size_t len; } httpd_ws_frame_t;
typedef void (*transfer_complete_cb)(esp_err_t err, int socket, void* arg);
esp_err_t httpd_ws_send_data_async(httpd_handle_t, int, httpd_ws_frame_t*, transfer_complete_cb, void*);
bool httpd_uri_match_wildcard(const char*, const char*, size_t);
esp_err_t httpd_get_client_list(httpd_handle_t, size_t*, int*);
esp_err_t httpd_sess_trigger_close(httpd_handle_t, int);
#define HTTPD_DEFAULT_CONFIG() {}
#ifdef __cplusplus
}
#endif
