// VesevOS Recovery - copia di riserva del firmware (vedi rec_riserva.h).
// Tocca solo app0: mai il recovery, la NVS delle impostazioni o i file. La copia sulla microSD non si cancella: serve anche la prossima volta.
#include "rec_riserva.h"
#include "rec_boot.h"
#include "rec_led.h"
#include "rec_order.h"
#include "rec_sd.h"
#include <stdio.h>
#include <esp_system.h>
#include <nvs.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define NS_RISERVA "vosris"

static uint8_t statoLetto(uint8_t* avvii = nullptr) {
  nvs_handle_t h;
  uint8_t st = RIS_VUOTA;
  if (nvs_open(NS_RISERVA, NVS_READONLY, &h) != ESP_OK) return st;
  nvs_get_u8(h, "st", &st);
  if (avvii) nvs_get_u8(h, "avv", avvii);
  nvs_close(h);
  return st;
}

static bool scrivi(const char* chiave, uint8_t valore) {
  nvs_handle_t h;
  if (nvs_open(NS_RISERVA, NVS_READWRITE, &h) != ESP_OK) return false;
  bool ok = nvs_set_u8(h, chiave, valore) == ESP_OK && nvs_commit(h) == ESP_OK;
  nvs_close(h);
  return ok;
}

void recRiservaDisarma() {
  nvs_handle_t h;
  if (nvs_open(NS_RISERVA, NVS_READWRITE, &h) != ESP_OK) return;
  nvs_erase_all(h);
  nvs_commit(h);
  nvs_close(h);
}

static bool leggiFile(RecSdFile& f, std::string& versione) {
  nvs_handle_t h;
  if (nvs_open(NS_RISERVA, NVS_READONLY, &h) != ESP_OK) return false;
  char testo[24] = "";
  size_t n = sizeof testo;
  size_t pin = sizeof f.pin, sha = sizeof f.sha;
  bool ok = nvs_get_str(h, "sdf", testo, &n) == ESP_OK && nvs_get_blob(h, "sdp", f.pin, &pin) == ESP_OK && pin == sizeof f.pin
         && nvs_get_blob(h, "sha", f.sha, &sha) == ESP_OK && sha == sizeof f.sha && nvs_get_u32(h, "dim", &f.size) == ESP_OK && f.size;
  f.file = testo;
  if (nvs_get_u32(h, "khz", &f.khz) != ESP_OK || f.khz < 400 || f.khz > 20000) f.khz = 4000;
  n = sizeof testo;
  if (nvs_get_str(h, "ver", testo, &n) == ESP_OK) versione = testo;
  nvs_close(h);
  return ok && recSdFileOk(f);
}

RisScelta recRiservaScelta() {
  uint8_t avvii = 0;
  uint8_t st = statoLetto(&avvii);
  if (st == RIS_VUOTA) return RIS_NIENTE;
  return risScegli(st, avvii, !app0Integra(), app0Scartata());
}

void recRiservaRiprova() {
  printf("[recovery] il firmware nuovo non si e confermato: secondo avvio\n");
  if (scrivi("avv", 1)) startApp();                            // se la NVS non si scrive niente secondo avvio: si rischierebbe un ciclo
  handFlag(true, false);
}

void recRiservaChiudi() {
  recResult(RES_WRITE, "ripristino interrotto");
  recRiservaDisarma();
  stayFlag(true, true);
  printf("[recovery] ripristino interrotto: resto nella pagina\n");
}

void recRiservaUsa() {
  if (statoLetto() != RIS_ARMATA) return;
  RecSdFile f;
  std::string versione;
  bool letta = leggiFile(f, versione);
  if (!versione.empty()) recOrder().ver = versione;            // l'esito deve nominare la versione scartata
  bool segnata = scrivi("st", RIS_USATA);                      // PRIMA di iniziare: un solo ripristino, anche se manca la corrente
  bool cancellata = false;
  int code = RES_WRITE;
  std::string why = "copia di riserva non valida";
  recLedSet(LS_WRITE);
  printf("[recovery] rimetto il firmware di prima dalla microSD\n");
  if (letta && segnata && recSdInstalla(f, false, nullptr, cancellata, code, why)) {
    recResult(RES_RIPRISTINO, "rimesso il firmware di prima");
    recRiservaDisarma();
    stayFlag(true, false);
    recLedSet(LS_DONE);
    recLedTick();
    printf("[recovery] firmware di prima rimesso: riavvio\n");
    vTaskDelay(pdMS_TO_TICKS(600));
    esp_restart();
  }
  recLedSet(LS_ERR);
  recResult(RES_WRITE, "riserva non usata: " + why);
  recRiservaDisarma();
  stayFlag(true, true);
}
