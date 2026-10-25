// VesevOS Recovery - "ordine di aggiornamento" lasciato da VesevOS nella NVS (spazio "vosupd") e esito.
// Contratto completo: docs/progetto/contratto-aggiornamento-recovery-2026-10-22.md
//   modo 1 = il recovery scarica da un indirizzo HTTPS senza pagina e senza login;
//   modo 2 = la pagina accetta il caricamento senza login con un gettone (uso singolo, scadenza, IP del client).
// L'ordine si legge e si CANCELLA subito (uso singolo). La scadenza parte dall'avvio del recovery (tempo monotono, niente orologio a muro).
#pragma once
#include <stdint.h>
#include <string>

enum RecResult {                       // codici dell'esito (chiave "res")
  RES_NONE = 0, RES_OK, RES_NET, RES_TLS, RES_SHA, RES_SIZE, RES_WRITE, RES_EXPIRED, RES_ORDER, RES_CANCEL
};

struct RecOrder {
  int mode = 0;                        // 0 = nessun ordine valido
  std::string url, ver;
  uint8_t sha[32] = {0};
  bool haveSha = false;
  uint32_t size = 0;
  uint8_t token[16] = {0};
  uint32_t cip = 0;                    // IPv4 del client autorizzato (ordine di rete), 0 = nessun controllo
  uint32_t t0 = 0, expMs = 0;          // avvio e durata
  int wrong = 0;                       // gettoni sbagliati finora
  bool burnt = false;                  // gettone consumato o bruciato
  bool used = false;                   // consumato da un'azione riuscita (upload o avvio), non bruciato dagli errori
};

void recOrderLoad();                   // all'avvio: legge, valida, cancella le chiavi dell'ordine
RecOrder& recOrder();
bool recOrderExpired();                // scaduto (scrive l'esito E_SCADUTO una sola volta)
void recResult(int code, const std::string& msg);   // scrive l'esito per VesevOS
std::string recOrderVer();

// Modo 2. tokHex = gettone dato dal browser (32 cifre esadecimali), ip = IPv4 del client (ordine di rete).
// Il gettone vale SOLO per tre azioni: leggere lo stato di app0 (/api/info), caricare l'immagine (/upload) e l'avvio normale (/api/boot).
bool recGrantCheck(const std::string& tokHex, uint32_t ip);   // vero = azione autorizzata senza login
void recGrantConsume();                // l'azione e riuscita (caricamento o avvio): il gettone non vale piu
// Frase in italiano per la pagina quando il gettone e stato rifiutato (scaduto, usato, annullato, sbagliato). Niente apici doppi: finisce in un JSON.
std::string recGrantDenyText();
