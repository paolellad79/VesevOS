// VesevOS - prove sul computer: quando rinfrescare lo spazio usato della memoria interna (drivers/vos_drv_fs_cache.h)
#include "vos_drv_fs_cache.h"
#include "mini.h"

int main() {
  FsUsedCache c;
  CHECK(!c.known && c.used == 0 && !c.dirty);
  // mai nei primi 20 s, nemmeno se non e mai stato calcolato
  CHECK(!c.due(0) && !c.due(1) && !c.due(FS_USED_FIRST_S - 1));
  CHECK(c.due(FS_USED_FIRST_S) && c.due(100000));                 // poi si': e il primo calcolo
  c.store(12345, 25);
  CHECK(c.known && c.used == 12345 && !c.dirty && c.lastSec == 25);
  // senza scritture: solo ogni FS_USED_EVERY_S (10 minuti)
  CHECK(!c.due(26) && !c.due(25 + FS_USED_EVERY_S - 1) && c.due(25 + FS_USED_EVERY_S));
  // con una scrittura: non prima di FS_USED_DIRTY_S (60 s) dall'ultimo calcolo
  c.touch();
  CHECK(c.dirty && !c.due(26) && !c.due(25 + FS_USED_DIRTY_S - 1) && c.due(25 + FS_USED_DIRTY_S));
  c.store(20000, 60);
  CHECK(!c.dirty && c.used == 20000 && !c.due(61) && !c.due(60 + FS_USED_DIRTY_S));   // pulito: i 60 s non bastano piu
  // molte scritture di fila non anticipano il calcolo
  for (int i = 0; i < 1000; i++) c.touch();
  CHECK(!c.due(60 + FS_USED_DIRTY_S - 1) && c.due(60 + FS_USED_DIRTY_S));
  // giro dei millisecondi (ogni 49,7 giorni i secondi ricominciano da 0): al piu un ricalcolo in piu, mai bloccato
  FsUsedCache w; w.store(1, 4294960u); w.touch();
  CHECK(!w.due(4294965u) && !w.due(5));                    // prima dei 20 s dal giro: niente
  CHECK(w.due(25));                                        // dopo: la differenza senza segno e enorme, si ricalcola una volta
  w.store(2, 25); CHECK(!w.due(26) && w.due(25 + FS_USED_EVERY_S));
  // il valore ricordato resta quello vecchio finche non si ricalcola (chi lo legge non scorre mai il file system)
  FsUsedCache v; v.store(777, 30); v.touch(); CHECK(v.used == 777);
  return done("fscache");
}
