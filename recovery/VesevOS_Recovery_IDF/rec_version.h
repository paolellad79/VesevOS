// VesevOS Recovery - versione e marchio.
// Il formato del marchio "{REC:VesevOS:" + versione + "}" NON si cambia: la pagina di VesevOS 1.7.58+ lo cerca
// (vedi VesevOS/src/core/vos_imgcheck.h) per aggiornare il recovery e leggerne la versione.
#pragma once

#define REC_VERSION "0.5.1"

// Tempi (millisecondi), uguali alla 0.4.4.
#define HANDOVER_MS 3000
#define STA_WAIT_MS 30000UL
#define IDLE_MS     900000UL     // 15 minuti senza accessi: se VesevOS e valido, ci torna da solo
#define TOKEN_MS    600000UL
#define NONCE_MS    60000UL
#define HASH_ITER   3000

// Pin del tasto BOOT.
#define BOOT_PIN 0
