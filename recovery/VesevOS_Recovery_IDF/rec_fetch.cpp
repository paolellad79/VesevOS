// VesevOS Recovery - scaricamento HTTPS (vedi rec_fetch.h). TLS con il bundle dei certificati di ESP-IDF; reindirizzamenti ammessi
// solo verso https (GitHub rimanda al sito dei file). Prima di impostare l'avvio: tipo, marchio, dimensione e SHA-256 (RecImage).
#include "rec_fetch.h"
#include "rec_boot.h"
#include "rec_image.h"
#include "rec_led.h"
#include "rec_order.h"
#include "rec_util.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <esp_netif_sntp.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define MAX_REDIRECTS 5
#define READ_CHUNK 2048

// I certificati hanno una data: senza orologio il controllo fallirebbe. Si chiede l'ora a pool.ntp.org (solo per questo ordine).
static bool clockReady() {
  if (time(nullptr) > 1700000000) return true;                  // gia sincronizzata
  esp_sntp_config_t c = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
  if (esp_netif_sntp_init(&c) != ESP_OK) return false;
  bool ok = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(15000)) == ESP_OK;
  esp_netif_sntp_deinit();
  return ok;
}

static int statusOf(esp_http_client_handle_t c) { return esp_http_client_get_status_code(c); }
static bool isRedirect(int s) { return s == 301 || s == 302 || s == 303 || s == 307 || s == 308; }

// Apre l'indirizzo seguendo i reindirizzamenti. 0 = pronto a leggere; altrimenti il codice RES_* dell'errore (motivo in why).
static int openFollowing(esp_http_client_handle_t c, std::string& why) {
  for (int hop = 0; hop <= MAX_REDIRECTS; hop++) {
    esp_err_t e = esp_http_client_open(c, 0);
    if (e != ESP_OK) { why = std::string("collegamento: ") + esp_err_to_name(e); return RES_TLS; }
    esp_http_client_fetch_headers(c);
    int s = statusOf(c);
    if (s == 200) return 0;
    if (!isRedirect(s)) { why = "HTTP " + std::to_string(s); return RES_NET; }
    int junk = 0;
    esp_http_client_flush_response(c, &junk);
    if (esp_http_client_set_redirection(c) != ESP_OK) { why = "reindirizzamento non valido"; return RES_NET; }
    char u[300] = "";
    esp_http_client_get_url(c, u, sizeof u);
    if (strncmp(u, "https://", 8) != 0) { why = "reindirizzamento non sicuro"; return RES_TLS; }
    esp_http_client_close(c);
  }
  why = "troppi reindirizzamenti";
  return RES_NET;
}

static void finish(int code, const std::string& why) { recResult(code, why); }

void recFetchRun() {
  RecOrder& o = recOrder();
  if (o.mode != 1) return;
  if (recOrderExpired()) return;
  printf("[recovery] modo 1: scarico %s (RAM libera %u, la piu piccola finora %u)\n", o.url.c_str(),
         (unsigned)esp_get_free_heap_size(), (unsigned)esp_get_minimum_free_heap_size());
  recLedSet(LS_WRITE);
  uint32_t t0 = recMillis();
  if (!clockReady()) { finish(RES_TLS, "ora non disponibile"); return; }

  esp_http_client_config_t cfg = {};
  cfg.url = o.url.c_str();
  cfg.crt_bundle_attach = esp_crt_bundle_attach;
  cfg.timeout_ms = 15000;
  cfg.buffer_size = 2048;
  cfg.buffer_size_tx = 1024;
  cfg.disable_auto_redirect = true;                              // i salti li gestiamo noi (controllo https)
  cfg.keep_alive_enable = false;
  esp_http_client_handle_t c = esp_http_client_init(&cfg);
  if (!c) { finish(RES_NET, "memoria"); return; }

  std::string why;
  int code = openFollowing(c, why);
  if (code) { esp_http_client_cleanup(c); recLedSet(LS_ERR); finish(code, why); return; }

  uint32_t wantSize = o.size;
  int64_t len = esp_http_client_get_content_length(c);
  if (wantSize && len > 0 && (uint32_t)len != wantSize) { esp_http_client_cleanup(c); recLedSet(LS_ERR); finish(RES_SIZE, "dimensione diversa da quella attesa"); return; }

  RecImage img(hexOf(o.sha, 32), wantSize);
  img.fileStart();
  static uint8_t buf[READ_CHUNK];
  size_t total = 0;
  for (;;) {
    int n = esp_http_client_read(c, (char*)buf, sizeof buf);
    if (n < 0) { img.fail(RES_NET, "lettura interrotta"); break; }
    if (n == 0) { if (esp_http_client_is_complete_data_received(c)) break; img.fail(RES_NET, "lettura interrotta"); break; }
    total += n;
    img.fileData(buf, n);
    if (img.failed()) break;
    recLedTick();
  }
  esp_http_client_close(c);
  esp_http_client_cleanup(c);
  if (!img.failed()) img.fileEnd();
  printf("[recovery] modo 1: %u byte in %u ms, RAM libera minima %u\n", (unsigned)total, (unsigned)(recMillis() - t0), (unsigned)esp_get_minimum_free_heap_size());
  if (img.failed()) { recLedSet(LS_ERR); finish(img.code(), img.err()); return; }

  stayFlag(true, false);
  finish(RES_OK, "scaricato");
  recLedSet(LS_DONE);
  recLedTick();
  printf("[recovery] firmware installato: riavvio\n");
  vTaskDelay(pdMS_TO_TICKS(600));
  esp_restart();
}
