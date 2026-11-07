// VesevOS Recovery - cosa fare della copia di riserva all'avvio. Logica pura, provata sul computer (tools/hosttest/prova_aggiorna.sh).
#pragma once
#include <stdint.h>

enum RisStato : uint8_t { RIS_VUOTA = 0, RIS_ARMATA = 1, RIS_USATA = 2 };   // RIS_USATA si scrive PRIMA di iniziare il ripristino

enum RisScelta { RIS_NIENTE, RIS_RIPROVA, RIS_RIPRISTINA, RIS_CHIUDI };

// appRotta = app0 non passa il controllo dell'immagine. appScartata = integra, ma il bootloader l'ha scartata (non si e confermata).
// Il firmware nuovo ha un secondo avvio: un'interruzione di corrente nei primi minuti non deve bastare a buttarlo.
inline RisScelta risScegli(uint8_t stato, uint8_t avvii, bool appRotta, bool appScartata) {
  if (stato == RIS_USATA) return RIS_CHIUDI;                   // ripristino interrotto: uno solo, niente cicli
  if (stato != RIS_ARMATA) return RIS_NIENTE;
  if (appRotta) return RIS_RIPRISTINA;
  if (!appScartata) return RIS_NIENTE;
  return avvii < 1 ? RIS_RIPROVA : RIS_RIPRISTINA;
}
