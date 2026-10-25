// microSD finta (vedi fake_sd.h): stessa interfaccia di drivers/vos_drv_sd.h
#include <Arduino.h>
#include "vos_drv_sd.h"
#include "fake_sd.h"
#include <string.h>

FakeSd g_sd;

void fakeSdReset() {
  g_sd.card = true;
  g_sd.gone = false;
  g_sd.failWrite = false;
  g_sd.corrupt = false;
  g_sd.up = false;
  g_sd.exfat = false;
  g_sd.badfs = false;
  g_sd.noResponse = false;
  g_sd.fs = DFS_FAT32;
  g_sd.lastFail = DSF_NONE;
  g_sd.begins = 0;
  g_sd.ends = 0;
  g_sd.writes = 0;
  g_sd.reads = 0;
  g_sd.proves = 0;
  g_sd.files.clear();
  g_sd.dirs.clear();
}

void fakeSdScenario(FakeSdScenario scenario) {
  bool wasUp = g_sd.up;

  if (!wasUp) {
    fakeSdReset();
  }

  g_sd.card = scenario != FSC_NO_CARD;
  g_sd.exfat = scenario == FSC_EXFAT;
  g_sd.badfs = scenario == FSC_BAD_FS;
  g_sd.noResponse = scenario == FSC_NO_RESPONSE;
  g_sd.gone = scenario == FSC_CARD_REMOVED;
  g_sd.failWrite = scenario == FSC_WRITE_FAILS;
}

static bool cardUsable() {
  return g_sd.up && !g_sd.gone;
}

bool drvSdBegin(int cs, int mosi, int sck, int miso, uint32_t hz) {
  g_sd.begins++;
  g_sd.cs = cs;
  g_sd.mosi = mosi;
  g_sd.sck = sck;
  g_sd.miso = miso;
  g_sd.hz = hz;

  if (g_sd.up) {
    return true;
  }

  g_sd.lastFail = DSF_NONE;

  if (!g_sd.card) {
    g_sd.lastFail = DSF_NO_CARD;
    return false;
  }
  if (g_sd.noResponse) {
    g_sd.lastFail = DSF_NO_RESPONSE;
    return false;
  }
  if (g_sd.exfat) {
    g_sd.lastFail = DSF_EXFAT;
    return false;
  }
  if (g_sd.badfs) {
    g_sd.lastFail = DSF_BAD_FS;
    return false;
  }

  g_sd.up = true;
  g_sd.gone = false;
  return true;
}

uint8_t drvSdLastFail() {
  return (uint8_t)g_sd.lastFail;
}

uint8_t drvSdFsType() {
  return g_sd.up ? (uint8_t)g_sd.fs : (uint8_t)DFS_UNKNOWN;
}

// Come il vero: scheda tolta = smontata da sola.
bool drvSdAlive() {
  if (g_sd.up && g_sd.gone) {
    g_sd.up = false;
    g_sd.ends++;
    return false;
  }

  return g_sd.up;
}

void drvSdEnd() {
  g_sd.ends++;
  g_sd.up = false;
}

bool drvSdUp() {
  return g_sd.up;
}

uint8_t drvSdType() {
  return g_sd.up ? DSD_SDHC : DSD_NONE;
}

uint64_t drvSdCardBytes() {
  return g_sd.up ? 8ULL * 1024 * 1024 * 1024 : 0;
}

uint64_t drvSdTotalBytes() {
  return g_sd.up ? 7ULL * 1024 * 1024 * 1024 : 0;
}

uint64_t drvSdUsedBytes() {
  uint64_t fileBytes = 0;

  for (auto& file : g_sd.files) {
    fileBytes += file.second.size();
  }

  return g_sd.up ? 100ULL * 1024 * 1024 + fileBytes : 0;
}

int drvSdRead(const char* path, uint8_t* buf, size_t max) {
  g_sd.reads++;

  if (!cardUsable()) {
    return -1;
  }

  auto it = g_sd.files.find(path);

  if (it == g_sd.files.end()) {
    return -1;
  }

  size_t n = it->second.size() < max ? it->second.size() : max;
  memcpy(buf, it->second.data(), n);
  return (int)n;
}

bool drvSdWrite(const char* path, const uint8_t* buf, size_t n, bool append) {
  g_sd.writes++;

  if (!cardUsable() || g_sd.failWrite) {
    return false;
  }

  std::string& content = g_sd.files[path];

  if (!append) {
    content.clear();
  }

  content.append((const char*)buf, n);
  return true;
}

// Aggiunge "full" all'elenco se sta direttamente dentro la cartella "prefix" (con la barra finale).
static void addListed(DrvSdEntry* out, int max, int& count, bool& more, const std::string& prefix,
                      const std::string& full, bool isDir, size_t size) {
  bool inFolder = full.compare(0, prefix.size(), prefix) == 0;
  bool direct = inFolder && full.find('/', prefix.size()) == std::string::npos;
  bool named = full.size() > prefix.size();

  if (!direct || !named) {
    return;
  }
  if (count >= max) {
    more = true;
    return;
  }

  DrvSdEntry& e = out[count];
  strncpy(e.name, full.c_str() + prefix.size(), sizeof e.name - 1);
  e.name[sizeof e.name - 1] = 0;
  e.dir = isDir;
  e.size = (uint32_t)size;
  count++;
}

int drvSdList(const char* dir, DrvSdEntry* out, int max, bool& more) {
  more = false;

  if (!cardUsable()) {
    return -1;
  }

  std::string folder = dir;

  if (folder != "/" && !g_sd.dirs.count(folder)) {
    return -1;
  }

  std::string prefix = folder == "/" ? "/" : folder + "/";
  int count = 0;

  for (auto& d : g_sd.dirs) {
    addListed(out, max, count, more, prefix, d, true, 0);
  }
  for (auto& f : g_sd.files) {
    addListed(out, max, count, more, prefix, f.first, false, f.second.size());
  }

  return count;
}

bool drvSdRemove(const char* path) {
  if (!cardUsable()) {
    return false;
  }

  return g_sd.files.erase(path) > 0;
}

bool drvSdMkdir(const char* path) {
  if (!cardUsable()) {
    return false;
  }

  g_sd.dirs.insert(path);
  return true;
}

// Il file di prova non resta mai sulla scheda finta.
uint8_t drvSdProve(uint32_t bytes, uint32_t& writeMs, uint32_t& readMs) {
  writeMs = 3;
  readMs = 2;

  if (!cardUsable()) {
    return DSP_NOT_UP;
  }
  if (g_sd.failWrite) {
    return DSP_NO_WRITE;
  }
  if (g_sd.corrupt) {
    return DSP_DIFFERENT;
  }

  g_sd.proves++;
  return DSP_OK;
}
