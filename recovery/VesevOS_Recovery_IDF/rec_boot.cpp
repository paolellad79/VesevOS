// VesevOS Recovery - app0, segni in NVS, passaggio a VesevOS, diagnostica (vedi rec_boot.h).
#include "rec_boot.h"
#include "rec_net.h"
#include "rec_version.h"
#include "recovery_check.h"
#include <stdio.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <nvs.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

const esp_partition_t* recApp0() {
  return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
}

bool app0Valid(char* name, size_t nl, char* ver, size_t vl) {
  const esp_partition_t* p = recApp0();
  if (!p) return false;
  uint8_t b = 0;
  if (esp_partition_read(p, 0, &b, 1) != ESP_OK || b != 0xE9) return false;
  esp_app_desc_t d;
  if (esp_ota_get_partition_description(p, &d) != ESP_OK) return false;
  snprintf(name, nl, "%s", d.project_name);
  snprintf(ver, vl, "%s", d.version);
  return true;
}

// Il firmware dentro app0 e VesevOS? Si cerca il marchio (mascherato, come per il file caricato). Risultato ricordato: -1 = da controllare.
static int8_t g_fwMark = -1;

static bool partRead(void* ctx, uint32_t off, uint8_t* buf, size_t n) {
  return esp_partition_read((const esp_partition_t*)ctx, off, buf, n) == ESP_OK;
}

bool app0Fw() {
  if (g_fwMark >= 0) return g_fwMark == 1;
  const esp_partition_t* p = recApp0();
  static uint8_t buf[2048];
  g_fwMark = (p && rcScanMark(partRead, (void*)p, p->size, buf, sizeof buf)) ? 1 : 0;
  return g_fwMark == 1;
}

void app0FwForget() { g_fwMark = -1; }
void app0FwSet(bool isFw) { g_fwMark = isFw ? 1 : 0; }

// ---- segni in NVS (spazio "vosrec"; sono valori da 1 byte come quelli scritti da VesevOS) ----
static bool nvsFlag(const char* key, bool set, bool val) {
  nvs_handle_t h;
  if (nvs_open("vosrec", set ? NVS_READWRITE : NVS_READONLY, &h) != ESP_OK) return false;
  bool r = false;
  if (set) {
    if (nvs_set_u8(h, key, val ? 1 : 0) == ESP_OK) nvs_commit(h);
    r = val;
  } else {
    uint8_t v = 0;
    r = (nvs_get_u8(h, key, &v) == ESP_OK) && v == 1;
  }
  nvs_close(h);
  return r;
}

bool stayFlag(bool set, bool val) { return nvsFlag("stay", set, val); }
bool handFlag(bool set, bool val) { return nvsFlag("hand", set, val); }

void startApp() {
  stayFlag(true, false);
  handFlag(true, true);
  const esp_partition_t* p = recApp0();
  if (!p) { printf("[recovery] app0 non trovata\n"); return; }
  esp_err_t e = esp_ota_set_boot_partition(p);
  if (e != ESP_OK) { printf("[recovery] errore nel passare ad app0: %d\n", (int)e); return; }
  printf("[recovery] passo a VesevOS...\n");
  fflush(stdout);
  vTaskDelay(pdMS_TO_TICKS(100));
  esp_restart();
}

// ---- diagnostica ----
static const char* rstName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "accensione";
    case ESP_RST_EXT: return "tasto RESET";
    case ESP_RST_SW: return "riavvio da programma";
    case ESP_RST_PANIC: return "ERRORE GRAVE (panic)";
    case ESP_RST_INT_WDT: return "watchdog (interrupt)";
    case ESP_RST_TASK_WDT: return "watchdog (task)";
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_DEEPSLEEP: return "uscita dal sonno";
    case ESP_RST_BROWNOUT: return "CALO DI TENSIONE";
    case ESP_RST_USB: return "USB";
    default: return "altro";
  }
}

static const char* imgState(const esp_partition_t* p, int* err) {
  esp_ota_img_states_t st;
  *err = 0;
  if (!p) return "n/d";
  esp_err_t e = esp_ota_get_state_partition(p, &st);
  if (e != ESP_OK) { *err = (int)e; return "senza stato"; }
  switch (st) {
    case ESP_OTA_IMG_NEW: return "nuova";
    case ESP_OTA_IMG_PENDING_VERIFY: return "in prova (non confermata)";
    case ESP_OTA_IMG_VALID: return "valida";
    case ESP_OTA_IMG_INVALID: return "SCARTATA (invalida)";
    case ESP_OTA_IMG_ABORTED: return "ABBANDONATA (rollback)";
    default: return "indefinita";
  }
}

// Perche siamo qui? Motivo dell'ultimo riavvio, partizione in esecuzione, partizione scelta dal bootloader (otadata), ultima scartata, stato di app0.
void recDiag() {
  const esp_partition_t *run = esp_ota_get_running_partition(), *bt = esp_ota_get_boot_partition(), *inv = esp_ota_get_last_invalid_partition();
  int er;
  const char* st = imgState(recApp0(), &er);
  printf("[recovery] diagnostica: riavvio per %s; in esecuzione 0x%X; scelta del bootloader 0x%X; ultima scartata %s; app0 %s (err %d); segno passaggio %s\n",
         rstName(esp_reset_reason()), run ? (unsigned)run->address : 0u, bt ? (unsigned)bt->address : 0u,
         inv ? "SI" : "nessuna", st, er, handFlag(false, false) ? "ACCESO" : "spento");
}

void recInfo() {
  char n[40] = "", v[40] = "";
  const esp_partition_t* p = recApp0();
  printf("\n[recovery %s] app0: ", REC_VERSION);
  if (p) printf("indirizzo 0x%X, dimensione %u KB, ", (unsigned)p->address, (unsigned)(p->size / 1024));
  else printf("partizione NON trovata, ");
  if (app0Valid(n, sizeof n, v, sizeof v)) printf("firmware \"%s\" versione %s%s\n", n, v, app0Fw() ? "" : " - ATTENZIONE: NON e VesevOS (manca il marchio)");
  else printf("nessun firmware valido\n");
  recDiag();
  recNetInfo();
}
