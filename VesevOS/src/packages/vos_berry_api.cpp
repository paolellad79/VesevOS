// Tabella delle funzioni Berry della scheda (vedi vos_berry_api.h).
// Il gruppo e il ruolo minimo seguono i controlli nei file vos_berry*.cpp
// (berryNeedFunc, needAdmin, berryBleRegister, ioSet).
#include "vos_berry_api.h"
#include "../core/vos_heapbuf.h"
#include <string.h>

const BerryFnInfo kFunzioniBerry[] = {
  // base
  {"led",           "base",  "operatore", "accende o spegne il LED"},
  {"act",           "base",  "operatore", "esegue un comando della shell"},
  {"btn",           "base",  "operatore", "click liberi del tasto BOOT (0 = nessuno)"},
  {"temp",          "base",  "operatore", "temperatura del chip in gradi"},
  {"cmd",           "base",  "operatore", "comando del driver (solo nei comandi)"},
  {"args",          "base",  "operatore", "argomenti del comando (solo nei comandi)"},
  // log e tempo
  {"log",           "log",   "operatore", "scrive una riga nel registro"},
  {"logtail",       "log",   "operatore", "legge le ultime righe del registro"},
  {"time",          "time",  "operatore", "ora attuale della scheda"},
  {"millis",        "time",  "operatore", "millisecondi dall'avvio"},
  {"sleep",         "time",  "operatore", "pausa in millisecondi"},
  {"uptime",        "time",  "operatore", "secondi dall'avvio"},
  {"delay_us",      "time",  "operatore", "pausa in microsecondi (massimo 10000)"},
  // rete
  {"mqtt",          "rete",  "operatore", "invia un messaggio MQTT"},
  {"scarica",       "rete",  "operatore", "scarica un file https (massimo 32 KB)"},
  // gpio, i2c, spi
  {"gpio_mode",     "gpio",  "admin",     "imposta la modalita di un pin"},
  {"gpio_read",     "gpio",  "operatore", "legge un pin"},
  {"gpio_write",    "gpio",  "admin",     "scrive su un pin"},
  {"i2c_probe",     "i2c",   "operatore", "dice se un dispositivo I2C risponde"},
  {"i2c_read",      "i2c",   "operatore", "legge dati da I2C"},
  {"i2c_write",     "i2c",   "admin",     "scrive dati su I2C"},
  {"spi_transfer",  "spi",   "admin",     "scambia dati su SPI"},
  // microSD
  {"sd_status",     "sd",    "operatore", "stato della microSD"},
  {"sd_read",       "sd",    "operatore", "legge un file dalla microSD"},
  {"sd_write",      "sd",    "admin",     "scrive un file sulla microSD"},
  {"sd_mount",      "sd",    "admin",     "monta la microSD"},
  {"sd_eject",      "sd",    "admin",     "smonta la microSD"},
  {"sd_format",     "sd",    "admin",     "formatta la microSD"},
  {"sd_mkdir",      "sd",    "admin",     "crea una cartella sulla microSD"},
  {"sd_rm",         "sd",    "admin",     "cancella un file o una cartella"},
  // file e dati delle app
  {"listdir",       "files", "operatore", "elenca il contenuto di una cartella"},
  {"stat",          "files", "operatore", "informazioni su un file o una cartella"},
  {"data_dir",      "files", "operatore", "cartella dei dati dell'app"},
  {"data_read",     "files", "operatore", "legge un file dei dati dell'app"},
  {"data_write",    "files", "operatore", "scrive un file dei dati dell'app"},
  {"data_rm",       "files", "operatore", "cancella un file dei dati dell'app"},
  // funzionalita e driver
  {"dev_get",       "dev",   "operatore", "legge un valore di un driver"},
  {"dev_set",       "dev",   "admin",     "imposta un valore di un driver (da verificare)"},
  // Bluetooth
  {"ble_status",    "ble",   "operatore", "stato del Bluetooth"},
  {"ble_on",        "ble",   "operatore", "accende il Bluetooth"},
  {"ble_off",       "ble",   "operatore", "spegne il Bluetooth"},
  {"ble_name",      "ble",   "operatore", "nome del Bluetooth"},
  {"ble_adv",       "ble",   "operatore", "annuncio Bluetooth"},
  {"ble_service",   "ble",   "operatore", "crea un servizio Bluetooth"},
  {"ble_set",       "ble",   "operatore", "imposta un valore Bluetooth"},
  {"ble_notify",    "ble",   "operatore", "invia una notifica Bluetooth"},
  {"ble_disconnect","ble",   "operatore", "chiude la connessione Bluetooth"},
  {"ble_passkey",   "ble",   "admin",     "codice di accoppiamento Bluetooth"},
};

const int kNFunzioniBerry = sizeof(kFunzioniBerry) / sizeof(kFunzioniBerry[0]);

// Sigla della scheda nel JSON. Il chip sta in boards/ (VOS_CHIP vale "ESP32-S3"): per C3, H2, C6 cambiare qui.
static const char kSiglaScheda[] = "S3";

// Ruolo in forma breve: "admin" oppure "op" (operatore).
static const char* ruoloBreve(const char* ruolo) {
  return strcmp(ruolo, "admin") == 0 ? "admin" : "op";
}

bool berryApiJson(HeapBuf& o) {
  o.add("{\"scheda\":\"");
  o.add(kSiglaScheda);
  o.add("\",\"funzioni\":[");
  for (int i = 0; i < kNFunzioniBerry; i++) {
    const BerryFnInfo& f = kFunzioniBerry[i];
    if (i > 0) o.add(',');
    o.add("{\"n\":\"");
    o.addJson(f.nome);
    o.add("\",\"g\":\"");
    o.addJson(f.gruppo);
    o.add("\",\"r\":\"");
    o.add(ruoloBreve(f.ruolo));
    o.add("\"}");
  }
  o.add("]}");
  return !o.oom() && !o.over();
}
