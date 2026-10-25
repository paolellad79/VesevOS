// VesevOS - TEST DI CONTRATTO della microSD (drivers/vos_drv_sd.h).
// Parla solo con il contratto: vale per qualunque backend "host" che offra gli scenari di fake_sd.h.
// Sulla scheda vera lo stesso giro lo fa la voce "sd" dell'autodiagnosi (vedi docs/progetto/da-fare-backend-sostituibili.md).
// Prova anche la logica comune ai backend (drivers/vos_drv_sd_common.cpp): riconoscimento del formato e prova di scrittura/lettura.
#include <Arduino.h>
#include "vos_drv_sd.h"
#include "fake_sd.h"
#include "mini.h"
#include <string.h>
#include <map>
#include <vector>

// vos_drv_sd_common.cpp e compilato con drvSdProve rinominata: cosi si prova anche con il backend finto.
uint8_t drvSdProveCommon(uint32_t bytes, uint32_t& writeMs, uint32_t& readMs);

static uint32_t g_clock = 0;

uint32_t drvSdMs() {
  g_clock += 5;
  return g_clock;
}

static const int CS = 10;
static const int MOSI = 11;
static const int SCK = 12;
static const int MISO = 13;
static const uint32_t HZ = 1000000;

static bool mountCard() {
  return drvSdBegin(CS, MOSI, SCK, MISO, HZ);
}

static bool writeText(const char* path, const char* text, bool append) {
  return drvSdWrite(path, (const uint8_t*)text, strlen(text), append);
}

static std::string readText(const char* path) {
  uint8_t buf[256];
  int n = drvSdRead(path, buf, sizeof buf);

  if (n < 0) {
    return "<errore>";
  }

  return std::string((const char*)buf, (size_t)n);
}

static bool listHas(const char* dir, const char* name, uint32_t* size) {
  DrvSdEntry entries[16];
  bool more = false;
  int n = drvSdList(dir, entries, 16, more);

  for (int i = 0; i < n; i++) {
    if (!strcmp(entries[i].name, name)) {
      if (size) {
        *size = entries[i].size;
      }
      return true;
    }
  }

  return false;
}

// Tutte le operazioni, a scheda non montata, devono rispondere "no" senza danni.
static void checkNothingWorks() {
  uint8_t buf[8];
  DrvSdEntry entries[2];
  bool more = true;
  uint32_t w = 0;
  uint32_t r = 0;

  CHECK(!drvSdUp());
  CHECK(!drvSdAlive());
  CHECK(drvSdType() == DSD_NONE);
  CHECK(drvSdCardBytes() == 0 && drvSdTotalBytes() == 0 && drvSdUsedBytes() == 0);
  CHECK(drvSdRead("/a.txt", buf, sizeof buf) == -1);
  CHECK(!writeText("/a.txt", "x", false));
  CHECK(drvSdList("/", entries, 2, more) == -1 && !more);
  CHECK(!drvSdRemove("/a.txt"));
  CHECK(!drvSdMkdir("/d"));
  CHECK(drvSdProveCommon(64, w, r) == DSP_NOT_UP);
}

static void testNoCard() {
  fakeSdScenario(FSC_NO_CARD);
  CHECK(!mountCard());
  CHECK(drvSdLastFail() == DSF_NO_CARD);
  checkNothingWorks();
  drvSdEnd();
  drvSdEnd();
  CHECK(!drvSdUp());
}

static void testBadCards() {
  fakeSdScenario(FSC_EXFAT);
  CHECK(!mountCard() && drvSdLastFail() == DSF_EXFAT);
  checkNothingWorks();

  fakeSdScenario(FSC_BAD_FS);
  CHECK(!mountCard() && drvSdLastFail() == DSF_BAD_FS);
  checkNothingWorks();

  fakeSdScenario(FSC_NO_RESPONSE);
  CHECK(!mountCard() && drvSdLastFail() == DSF_NO_RESPONSE);
  checkNothingWorks();
}

static void testMountAndInfo() {
  fakeSdScenario(FSC_CARD_OK);
  CHECK(mountCard());
  CHECK(drvSdUp() && drvSdAlive());
  CHECK(drvSdLastFail() == DSF_NONE);
  CHECK(drvSdFsType() == DFS_FAT32);
  CHECK(drvSdType() != DSD_NONE);
  CHECK(drvSdCardBytes() > 0);
  CHECK(drvSdTotalBytes() > 0 && drvSdTotalBytes() <= drvSdCardBytes());
  CHECK(drvSdUsedBytes() <= drvSdTotalBytes());
  CHECK(mountCard());    // montare due volte non fa danni
}

static void testFiles() {
  CHECK(drvSdMkdir("/dir"));
  CHECK(writeText("/dir/a.txt", "ciao", false));
  CHECK(readText("/dir/a.txt") == "ciao");

  CHECK(writeText("/dir/a.txt", " mondo", true));
  CHECK(readText("/dir/a.txt") == "ciao mondo");

  CHECK(writeText("/dir/a.txt", "nuovo", false));
  CHECK(readText("/dir/a.txt") == "nuovo");

  uint32_t size = 0;
  CHECK(listHas("/", "dir", nullptr));
  CHECK(listHas("/dir", "a.txt", &size) && size == 5);

  uint8_t small[3];
  CHECK(drvSdRead("/dir/a.txt", small, sizeof small) == 3);    // mai oltre "max"
  CHECK(drvSdRead("/non-esiste.txt", small, sizeof small) == -1);
  CHECK(drvSdRead("/dir", small, sizeof small) == -1);         // una cartella non si legge

  DrvSdEntry one[1];
  bool more = false;
  CHECK(writeText("/dir/b.txt", "b", false));
  CHECK(drvSdList("/dir", one, 1, more) == 1 && more);         // ce ne sono altre
  CHECK(drvSdList("/non-esiste", one, 1, more) == -1);

  CHECK(drvSdRemove("/dir/a.txt"));
  CHECK(readText("/dir/a.txt") == "<errore>");
  CHECK(!drvSdRemove("/dir/a.txt"));                           // gia cancellato
}

static void testProve() {
  uint32_t w = 0;
  uint32_t r = 0;

  CHECK(drvSdProveCommon(4096, w, r) == DSP_OK);
  CHECK(w > 0 && r > 0);
  CHECK(readText("/vesevos-prova.tmp") == "<errore>");         // il file di prova non resta

  fakeSdScenario(FSC_WRITE_FAILS);
  CHECK(drvSdProveCommon(4096, w, r) == DSP_NO_WRITE);
  fakeSdScenario(FSC_CARD_OK);
}

static void testUnmountAndRemoval() {
  drvSdEnd();
  CHECK(!drvSdUp());
  checkNothingWorks();
  drvSdEnd();

  fakeSdScenario(FSC_CARD_OK);
  CHECK(mountCard());
  fakeSdScenario(FSC_CARD_REMOVED);
  CHECK(!drvSdAlive());                                        // scheda tolta: smonta da sola
  CHECK(!drvSdUp());
  drvSdEnd();
}

// ---- Riconoscimento del formato dai primi settori (logica comune ai backend) ----

struct Disk {
  std::map<uint32_t, std::vector<uint8_t>> sectors;
};

static bool readFromDisk(void* ctx, uint8_t* buf, uint32_t lba) {
  Disk* disk = (Disk*)ctx;
  auto it = disk->sectors.find(lba);

  if (it == disk->sectors.end()) {
    return false;
  }

  memcpy(buf, it->second.data(), 512);
  return true;
}

static std::vector<uint8_t> bootSector(const char* fsName, int nameOffset) {
  std::vector<uint8_t> s(512, 0);
  s[0] = 0xEB;
  memcpy(&s[nameOffset], fsName, 8);
  s[510] = 0x55;
  s[511] = 0xAA;
  return s;
}

static std::vector<uint8_t> mbrWithPartitionAt(uint32_t lba) {
  std::vector<uint8_t> s(512, 0);
  s[446 + 4] = 0x0C;
  s[446 + 8] = (uint8_t)(lba & 0xFF);
  s[446 + 9] = (uint8_t)((lba >> 8) & 0xFF);
  s[510] = 0x55;
  s[511] = 0xAA;
  return s;
}

static void probe(Disk& disk, uint8_t& fs, uint8_t& sector0) {
  uint8_t work[512];
  drvSdProbeFs(readFromDisk, &disk, work, fs, sector0);
}

static void testProbeFs() {
  uint8_t fs = 0;
  uint8_t s0 = 0;

  Disk fat32;
  fat32.sectors[0] = bootSector("FAT32   ", 82);
  probe(fat32, fs, s0);
  CHECK(fs == DFS_FAT32 && s0 == DS0_VALID);

  Disk fat16;
  fat16.sectors[0] = bootSector("FAT16   ", 54);
  probe(fat16, fs, s0);
  CHECK(fs == DFS_FAT16 && s0 == DS0_VALID);

  Disk fat12;
  fat12.sectors[0] = bootSector("FAT12   ", 54);
  probe(fat12, fs, s0);
  CHECK(fs == DFS_FAT12 && s0 == DS0_VALID);

  Disk exfat;
  exfat.sectors[0] = bootSector("EXFAT   ", 3);
  probe(exfat, fs, s0);
  CHECK(fs == DFS_EXFAT && s0 == DS0_VALID);

  Disk partitioned;
  partitioned.sectors[0] = mbrWithPartitionAt(8192);
  partitioned.sectors[8192] = bootSector("FAT32   ", 82);
  probe(partitioned, fs, s0);
  CHECK(fs == DFS_FAT32 && s0 == DS0_VALID);

  Disk brokenPartition;
  brokenPartition.sectors[0] = mbrWithPartitionAt(8192);
  probe(brokenPartition, fs, s0);
  CHECK(fs == DFS_UNKNOWN && s0 == DS0_VALID);

  Disk stuckHigh;
  stuckHigh.sectors[0] = std::vector<uint8_t>(512, 0xFF);
  probe(stuckHigh, fs, s0);
  CHECK(fs == DFS_UNKNOWN && s0 == DS0_EMPTY);

  Disk stuckLow;
  stuckLow.sectors[0] = std::vector<uint8_t>(512, 0x00);
  probe(stuckLow, fs, s0);
  CHECK(s0 == DS0_EMPTY);

  Disk noSignature;
  noSignature.sectors[0] = std::vector<uint8_t>(512, 0x5A);
  probe(noSignature, fs, s0);
  CHECK(fs == DFS_UNKNOWN && s0 == DS0_NO_SIGNATURE);

  Disk unreadable;
  probe(unreadable, fs, s0);
  CHECK(fs == DFS_UNKNOWN && s0 == DS0_UNREADABLE);
}

int main() {
  testNoCard();
  testBadCards();
  testMountAndInfo();
  testFiles();
  testProve();
  testUnmountAndRemoval();
  testProbeFs();
  return done("sd_contract");
}
