// VesevOS Recovery - installazione dalla microSD (vedi rec_sd.h).
// Prima si legge tutto il file: dimensione, SHA-256, intestazione, marchio. Solo se e giusto si tocca app0.
// Il file si rilegge scrivendo: RecImage rifa' gli stessi controlli, cosi un errore di lettura non diventa un firmware rotto.
#include "rec_sd.h"
#include "rec_boot.h"
#include "rec_image.h"
#include "rec_led.h"
#include "rec_order.h"
#include "rec_riserva.h"
#include "rec_util.h"
#include "recovery_check.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <driver/sdspi_host.h>
#include <driver/spi_common.h>
#include <esp_system.h>
#include <esp_vfs_fat.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mbedtls/sha256.h>
#include <sdmmc_cmd.h>

#define SD_BASE "/sd"
#define SD_BLOCCO 4096

static const spi_host_device_t SD_HOST = SPI2_HOST;
static sdmmc_card_t* g_carta = nullptr;
static bool g_bus = false;
static uint8_t g_buf[SD_BLOCCO];

static bool monta(const RecSdFile& o) {
  spi_bus_config_t bus = {};
  bus.mosi_io_num = o.pin[1];
  bus.sclk_io_num = o.pin[2];
  bus.miso_io_num = o.pin[3];
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = SD_BLOCCO;
  if (spi_bus_initialize(SD_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) return false;
  g_bus = true;

  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  host.slot = SD_HOST;
  host.max_freq_khz = (int)o.khz;
  sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
  slot.gpio_cs = (gpio_num_t)o.pin[0];
  slot.host_id = SD_HOST;
  esp_vfs_fat_mount_config_t m = {};
  m.format_if_mount_failed = false;                    // mai formattare: sulla scheda ci sono i dati dell'utente
  m.max_files = 1;
  m.allocation_unit_size = 16 * 1024;
  esp_err_t e = esp_vfs_fat_sdspi_mount(SD_BASE, &host, &slot, &m, &g_carta);
  if (e != ESP_OK) { printf("[recovery] microSD non montata: %s\n", esp_err_to_name(e)); g_carta = nullptr; }
  return e == ESP_OK;
}

static void smonta() {
  if (g_carta) esp_vfs_fat_sdcard_unmount(SD_BASE, g_carta);
  g_carta = nullptr;
  if (g_bus) spi_bus_free(SD_HOST);
  g_bus = false;
}

// Legge tutto il file. false = motivo in why e codice in code; app0 non e stata toccata.
static bool fileGiusto(FILE* f, const RecSdFile& o, int& code, std::string& why) {
  struct stat st;
  if (fstat(fileno(f), &st) != 0 || (uint32_t)st.st_size != o.size) { code = RES_SIZE; why = "dimensione diversa da quella attesa"; return false; }
  RecCheck chk;
  rcInit(chk);
  mbedtls_sha256_context c;
  mbedtls_sha256_init(&c);
  mbedtls_sha256_starts(&c, 0);
  size_t n, tot = 0;
  while ((n = fread(g_buf, 1, sizeof g_buf, f)) > 0) {
    rcFeed(chk, g_buf, n);
    mbedtls_sha256_update(&c, g_buf, n);
    tot += n;
    recLedTick();
    vTaskDelay(1);
  }
  bool letto = !ferror(f) && tot == o.size;
  uint8_t dg[32];
  mbedtls_sha256_finish(&c, dg);
  mbedtls_sha256_free(&c);
  if (!letto) { code = RES_CANCEL; why = "microSD non leggibile"; return false; }
  if (hexOf(dg, 32) != hexOf(o.sha, 32)) { code = RES_SHA; why = "SHA-256 diverso da quello atteso"; return false; }
  const esp_partition_t* p = recApp0();
  const char* e = rcFinalErr(chk, p ? p->size : 0);
  if (e) { code = RES_SIZE; why = e; return false; }
  return true;
}

static bool scrivi(FILE* f, const RecSdFile& o, int& code, std::string& why) {
  RecImage img(hexOf(o.sha, 32), o.size);
  img.fileStart();
  size_t n;
  while ((n = fread(g_buf, 1, sizeof g_buf, f)) > 0) {
    img.fileData(g_buf, n);
    if (img.failed()) break;
    recLedTick();
    vTaskDelay(1);
  }
  if (!img.failed() && ferror(f)) img.fail(RES_WRITE, "microSD non leggibile durante la scrittura");
  if (!img.failed()) img.fileEnd();
  if (img.failed()) { code = img.code(); why = img.err(); return false; }
  return true;
}

bool recSdInstalla(const RecSdFile& s, bool togliFile, void (*primaDi)(), bool& cancellata, int& code, std::string& why) {
  cancellata = false;
  code = RES_CANCEL;
  why = "microSD non montata";
  bool ok = false;
  if (monta(s)) {
    std::string percorso = std::string(SD_BASE) + s.file;
    FILE* f = fopen(percorso.c_str(), "rb");
    if (!f) why = "file non trovato sulla microSD";
    else {
      if (fileGiusto(f, s, code, why)) {
        if (primaDi) primaDi();                        // dopo c'e la cancellazione di app0
        cancellata = true;
        rewind(f);
        ok = scrivi(f, s, code, why);
      }
      fclose(f);                                       // prima di cancellare: un file aperto non si toglie
    }
    if (ok && togliFile) unlink(percorso.c_str());     // se non riesce lo toglie VesevOS al prossimo controllo
  }
  smonta();
  return ok;
}

void recSdRun() {
  RecOrder& o = recOrder();
  if (o.mode != 3) return;
  recLedSet(LS_WRITE);
  RecSdFile s;
  s.file = o.sdFile;
  memcpy(s.pin, o.sdPin, sizeof s.pin);
  s.khz = o.sdKhz;
  memcpy(s.sha, o.sha, sizeof s.sha);
  s.size = o.size;
  bool cancellata = false;
  int code = RES_CANCEL;
  std::string why;
  if (!recSdInstalla(s, true, recOrderTentativo, cancellata, code, why)) {
    recLedSet(LS_ERR);
    recResult(code, why);
    recOrderChiudi();
    if (cancellata) recRiservaUsa();                   // app0 e persa: se c'e la riserva si rimette, se riesce non torna qui
    else recRiservaDisarma();                          // app0 e intatta: si torna a VesevOS
    return;
  }
  stayFlag(true, false);
  recResult(RES_OK, "installato dalla microSD");
  recOrderChiudi();
  recLedSet(LS_DONE);
  recLedTick();
  printf("[recovery] firmware installato dalla microSD: riavvio\n");
  vTaskDelay(pdMS_TO_TICKS(600));
  esp_restart();
}
