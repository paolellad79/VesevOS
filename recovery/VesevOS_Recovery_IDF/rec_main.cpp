// VesevOS Recovery - ESP-IDF puro. Stesse funzioni della 0.4.4, piu gli ordini di aggiornamento (modo 1 e 2).
// Cosa fa:
//  - Se VesevOS e valido e nessuno ha chiesto il recovery: dopo 3 s passa a VesevOS.
//  - Se VesevOS ha chiesto il recovery (`recovery now`), oppure app0 non e valido, oppure BOOT premuto all'accensione:
//    resta qui, si collega alla rete di casa (impostazioni lette da /vesevos.conf in LittleFS) e, se non ci riesce in 30 s,
//    apre l'hotspot 'VesevOS-recovery'. Pagina web: con utente e password di un amministratore di VesevOS carichi il .bin.
//  - Se non esiste nessun amministratore (flash vuota): serve il codice a 8 cifre mostrato sulla seriale.
// Protezioni (dalla 0.4.x): se l'ultima volta il passaggio a VesevOS e finito con un ritorno qui, resta fermo; se in app0 non c'e il marchio
// di VesevOS non ci passa (il comando seriale b forza).
// Ordini di VesevOS (contratto: docs/progetto/contratto-aggiornamento-recovery-2026-10-22.md): modo 1 = scarica da HTTPS senza pagina;
// modo 2 = la pagina accetta il caricamento senza login con un gettone.
// Comandi sulla seriale (115200): i = info, b = avvia VesevOS (anche senza marchio), r = riavvia.
#include "rec_version.h"
#include "rec_boot.h"
#include "rec_config.h"
#include "rec_led.h"
#include "rec_net.h"
#include "rec_fetch.h"
#include "rec_order.h"
#include "rec_util.h"
#include "rec_web.h"
#include <stdio.h>
#include <driver/gpio.h>
#include <driver/usb_serial_jtag.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Marchio del recovery: "{REC:VesevOS:" + versione + "}". "used" lo tiene nel file anche dopo l'ottimizzazione.
static const char REC_MARK[] __attribute__((used)) = "{REC:VesevOS:" REC_VERSION "}";

static void serialStart() {
  usb_serial_jtag_driver_config_t c = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
  c.rx_buffer_size = 256;
  c.tx_buffer_size = 256;
  usb_serial_jtag_driver_install(&c);
}

static void handleSerial() {
  uint8_t c;
  while (usb_serial_jtag_read_bytes(&c, 1, 0) > 0) {
    if (c == 'i') recInfo();
    else if (c == 'b') startApp();
    else if (c == 'r') { printf("[recovery] riavvio\n"); vTaskDelay(pdMS_TO_TICKS(100)); esp_restart(); }
  }
}

static const char* stayReason(bool notFw, bool back, bool stay, bool boot) {
  if (notFw) return "in app0 non c'e VesevOS (manca il marchio): resto qui, carica il firmware dalla pagina o con carica-firmware.sh";
  if (back) return "il passaggio a VesevOS non ha funzionato (siamo tornati qui): resto qui per non girare a vuoto. Premi RESET per riprovare, oppure carica un firmware dalla pagina";
  if (stay) return "richiesto da VesevOS: resto qui";
  if (boot) return "tasto BOOT premuto: resto qui";
  return "niente da avviare: resto qui";
}

// Ciclo principale: LED, comandi seriali, ritorno a VesevOS dopo 15 minuti senza accessi.
static void mainLoop() {
  uint32_t lastMsg = 0;
  for (;;) {
    recLedTick();
    handleSerial();
    recOrderExpired();                                // se l'ordine e scaduto scrive l'esito
    uint32_t now = recMillis();
    if (recNetUp() && now - recWebLastActivity() > IDLE_MS) {
      char n[40], v[40];
      if (app0Valid(n, sizeof n, v, sizeof v) && app0Fw()) { printf("[recovery] nessun accesso da 15 minuti: torno a VesevOS\n"); startApp(); }
      recWebTouch();
    }
    if (now - lastMsg > 15000) { lastMsg = now; printf("[recovery] in attesa (i = info, b = avvia VesevOS, r = riavvia)\n"); }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

extern "C" void app_main(void) {
  (void)*(volatile const char*)REC_MARK;
  serialStart();
  esp_err_t ne = nvs_flash_init();                    // MAI cancellare la NVS da qui: ci sono le impostazioni di VesevOS
  if (ne != ESP_OK) printf("[recovery] NVS non pronta (%s): i segni di avvio non funzionano\n", esp_err_to_name(ne));
  gpio_config_t g = {};
  g.pin_bit_mask = 1ULL << BOOT_PIN;
  g.mode = GPIO_MODE_INPUT;
  g.pull_up_en = GPIO_PULLUP_ENABLE;
  gpio_config(&g);
  vTaskDelay(pdMS_TO_TICKS(1500));                    // tempo per aprire il monitor seriale
  printf("\n\n=== VesevOS Recovery %s ===\n", REC_VERSION);

  char n[40], v[40];
  bool ok = app0Valid(n, sizeof n, v, sizeof v);
  bool notFw = ok && !app0Fw();                       // c'e un programma in app0 ma non e VesevOS (es. un altro recovery)
  if (notFw) ok = false;
  recOrderLoad();                                     // ordine di VesevOS (aggiornamento online o con gettone), uso singolo
  bool stay = stayFlag(false, false) || recOrder().mode != 0;
  bool boot = gpio_get_level((gpio_num_t)BOOT_PIN) == 0;
  bool back = handFlag(false, false);                 // l'ultima volta siamo passati a VesevOS ma siamo tornati qui: non riprovare all'infinito
  recInfo();
  if (back) handFlag(true, false);                    // riprova al prossimo accensione/RESET
  if (ok && !stay && !boot && !back) {
    printf("[recovery] avvio %s tra %d s (tieni premuto BOOT all'accensione per restare qui)\n", n, HANDOVER_MS / 1000);
    vTaskDelay(pdMS_TO_TICKS(HANDOVER_MS));
    startApp();
  }
  printf("[recovery] %s\n", stayReason(notFw, back, stay, boot));
  loadConfig();
  recLedStart(rc.ledPin, rc.ledBr);
  recLedSet(LS_WAIT);
  recNetStart();
  if (recOrder().mode == 1) {                         // aggiornamento online invisibile: se non riesce, resta la pagina normale
    if (recNetIsAp()) recResult(RES_NET, "rete di casa non raggiunta");
    else recFetchRun();
  }
  recWebStart();
  mainLoop();
}
