// VesevOS - prova sul computer dei PERCORSI della microSD: core/vos_paths.cpp VERO (il layout e finto). La lancia prova_sdpath.sh.
// Prova: parte sulla scheda (pathSdRel), cartella virtuale /media, nomi ammessi dalla scheda (pathSdRelOk), file di sistema anche sulla scheda.
#include <stdio.h>
#include <string.h>
#include "vos_paths.h"
#include "vos_layout.h"
#include "mini.h"

const char* layoutPathNew(LayoutItem) { return "/n"; }
const char* layoutPathOld(LayoutItem) { return "/o"; }
int layoutState() { return 0; }

int main() {
  // 1. parte sulla scheda e cartella virtuale
  CHECK(pathSdRel("/media/sd") && !strcmp(pathSdRel("/media/sd"), "/"));
  CHECK(!strcmp(pathSdRel("/media/sd/app/x"), "/app/x"));
  CHECK(!pathSdRel("/media/sdcard") && !pathSdRel("/media") && !pathSdRel("/sd") && !pathSdRel("/var"));
  CHECK(pathIsMedia("/media") && pathIsMedia("/media/sd/a") && pathIsMedia("/media/x") && !pathIsMedia("/mediax"));
  CHECK(pathIsMediaRoot("/media") && !pathIsMediaRoot("/media/sd"));
  // 2. nomi ammessi
  CHECK(pathSdRelOk("/") && pathSdRelOk("/app/cam 1/foto_(2).jpg") && pathSdRelOk("/a-b.txt"));
  CHECK(!pathSdRelOk("") && !pathSdRelOk("a") && !pathSdRelOk(nullptr));
  CHECK(!pathSdRelOk("/a//b") && !pathSdRelOk("/a/../b") && !pathSdRelOk("/a/") && !pathSdRelOk("/a."));
  CHECK(!pathSdRelOk("/a ") && !pathSdRelOk("/ a") && !pathSdRelOk("/a /b") && !pathSdRelOk("/a./b"));
  CHECK(!pathSdRelOk("/a:b") && !pathSdRelOk("/a*b") && !pathSdRelOk("/a?b") && !pathSdRelOk("/a~b") && !pathSdRelOk("/a\"b"));
  char lungo[200]; lungo[0] = '/'; memset(lungo + 1, 'a', 100); lungo[101] = 0;
  CHECK(!pathSdRelOk(lungo));                                    // 101 caratteri
  lungo[100] = 0;
  CHECK(pathSdRelOk(lungo));                                     // 100
  // 3. file di sistema anche sulla scheda
  CHECK(pathIsSystemFile("/media/sd/vesevos.conf") && pathIsSystemFile("/media/sd/audit.log") && !pathIsSystemFile("/media/sd/foto.jpg"));
  CHECK(pathIsSecret("/media/sd/chiave.pem") && pathIsSecret("/media/sd/a.VBK") && !pathIsSecret("/media/sd/a.txt"));
  // 4. la scorciatoia /sd
  char b[64]; snprintf(b, sizeof b, "/sd/x");
  CHECK(pathSdShortcut(b, sizeof b, 100) && !strcmp(b, "/media/sd/x"));
  return done("prova_sdpath");
}
