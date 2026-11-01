// VesevOS - prova sul computer della CONFIGURAZIONE dei programmi: sottosistemi (vdb, vde), Funzionalita tolte e programmi con avvio
// automatico. Compila i file veri core/vos_config.cpp, vos_config_import.cpp e vos_config_export.cpp con pochi finti. La lancia prova_cfgprog.sh.
// Prova: valori di fabbrica (tutto acceso, nulla tolto, nessun programma parte da solo), andata e ritorno esporta -> importa, id sconosciuti,
// elenco troppo lungo, file di configurazione vecchio senza le sezioni nuove (restano i valori di fabbrica).
#include <stdio.h>
#include <string.h>
#include <string>
#include "vos_config.h"
#include "vos_drv_fs.h"
#include "vos_drv_nvs.h"
#include "vos_func_ids.h"
#include "mini.h"

// ---- finti: tutto cio che la configurazione chiede fuori da se
const char* tr(const char* it) { return it; }
void vlog(const char*, ...) {}
void auditDiff(const char*, const char*, const char*) {}
void auditFix() {}
void auditRefresh() {}
bool cpuGovParamsValid(int, int, int) { return true; }
bool domainValid(std::string_view) { return true; }
bool hostnameValid(std::string_view) { return true; }
bool langCodeValid(std::string_view) { return true; }
bool ipParse(std::string_view, uint32_t&) { return false; }
uint32_t esp_random() { return 4; }
bool layoutEnsureDirs() { return true; }
bool layoutWriteAtomic(const char*, const uint8_t*, size_t) { return true; }
void sha256HexOf(std::string_view, char* out) { strcpy(out, "0"); }
// memoria interna e NVS finte: nessun file, nessuna chiave (la prova usa solo cfgExport e cfgImport)
bool drvFsExists(const char*) { return false; }
bool drvFsRemove(const char*) { return false; }
bool drvFsRename(const char*, const char*) { return false; }
VosFile drvFsOpen(const char*, const char*) { return VosFile(); }
void VosFile::close() {}
size_t VosFile::read(uint8_t*, size_t) { return 0; }
size_t VosFile::write(const uint8_t*, size_t) { return 0; }
bool DrvNvs::begin(const char*, bool) { return false; }
void DrvNvs::end() {}
size_t DrvNvs::getString(const char*, char*, size_t) { return 0; }
void DrvNvs::putString(const char*, const char*) {}
void DrvNvs::remove(const char*) {}
void DrvNvs::clear() {}

static std::string esporta() {
  HeapBuf h(CFG_TEXT_MAX);
  bool ok = cfgExport(h, false);
  CHECK(ok);
  return std::string(h.c_str(), h.length());
}

int main() {
  cfgDefaults();
  // 1. valori di fabbrica
  CHECK(cfg.berryOn && cfg.elfOn && cfg.funcOff == 0 && cfg.bootN == 0);
  // 2. andata e ritorno
  cfg.berryOn = false;
  cfg.elfOn = false;
  cfg.funcOff = (uint16_t)((1u << funcIndex("sd")) | (1u << funcIndex("gpio")));
  cfg.bootN = 2;
  cfg.boot[0].kind = 0; strcpy(cfg.boot[0].name, "lampeggio");
  cfg.boot[1].kind = 2; strcpy(cfg.boot[1].name, "sensore");
  std::string t = esporta();
  CHECK(t.find("config vdb 'vdb'") != std::string::npos && t.find("config func 'func'") != std::string::npos && t.find("config progboot 'progboot'") != std::string::npos);
  CHECK(t.find("option off 'sd,gpio'") != std::string::npos);
  CHECK(t.find("option p1 's:lampeggio'") != std::string::npos && t.find("option p2 'e:sensore'") != std::string::npos);
  cfgDefaults();
  ErrBuf e;
  CHECK(cfgImport(t.c_str(), t.size(), e));
  CHECK(!cfg.berryOn && !cfg.elfOn);
  CHECK(cfg.funcOff == (uint16_t)((1u << funcIndex("sd")) | (1u << funcIndex("gpio"))));
  CHECK(cfg.bootN == 2 && cfg.boot[0].kind == 0 && !strcmp(cfg.boot[0].name, "lampeggio") && cfg.boot[1].kind == 2 && !strcmp(cfg.boot[1].name, "sensore"));
  // 3. importare di nuovo lo stesso testo SOSTITUISCE l'elenco (non lo allunga)
  CHECK(cfgImport(t.c_str(), t.size(), e) && cfg.bootN == 2);
  // 4. app Berry e id sconosciuti
  std::string u = "config func 'func'\n\toption off 'ble,nonesiste,,time'\nconfig progboot 'progboot'\n\toption p1 'b:app1'\n";
  CHECK(cfgImport(u.c_str(), u.size(), e));
  CHECK(cfg.funcOff == (uint16_t)((1u << funcIndex("ble")) | (1u << funcIndex("time"))));
  CHECK(cfg.bootN == 1 && cfg.boot[0].kind == 1 && !strcmp(cfg.boot[0].name, "app1"));
  // 5. tutti spenti e poi tutti concessi
  std::string off = "config func 'func'\n\toption off 'log,sd,dev,gpio,i2c,spi,time,files,ble'\n";
  CHECK(cfgImport(off.c_str(), off.size(), e) && cfg.funcOff == (1u << kFuncN) - 1);
  std::string on = "config func 'func'\n\toption off ''\n";
  CHECK(cfgImport(on.c_str(), on.size(), e) && cfg.funcOff == 0);
  // 6. elenco di avvio troppo lungo: si tengono i primi 8; nomi troppo lunghi e voci storte ignorati
  std::string big = "config progboot 'progboot'\n";
  for (int i = 1; i <= 10; i++) big += "\toption p" + std::to_string(i) + " 's:prog" + std::to_string(i) + "'\n";
  big += "\toption p11 's:" + std::string(30, 'x') + "'\n\toption pz 'zz'\n";
  CHECK(cfgImport(big.c_str(), big.size(), e));
  CHECK(cfg.bootN == VOS_BOOTPROG_MAX && !strcmp(cfg.boot[0].name, "prog1") && !strcmp(cfg.boot[VOS_BOOTPROG_MAX - 1].name, "prog8"));
  // 7. un file di configurazione vecchio, senza le sezioni nuove: restano i valori che c'erano (di fabbrica dopo cfgDefaults)
  cfgDefaults();
  std::string old = "config system 'system'\n\toption hostname 'vesevos9'\n";
  CHECK(cfgImport(old.c_str(), old.size(), e));
  CHECK(cfg.berryOn && cfg.elfOn && cfg.funcOff == 0 && cfg.bootN == 0);
  // 8. app che salvano sulla microSD (sezione progdata): di fabbrica nessuna, andata e ritorno, sostituisce l'elenco, voci storte ignorate, massimo 8
  CHECK(cfg.dataSdN == 0);
  cfg.dataSdN = 2;
  cfg.dataSd[0].kind = 1; strcpy(cfg.dataSd[0].name, "cam");
  cfg.dataSd[1].kind = 2; strcpy(cfg.dataSd[1].name, "logger");
  std::string d = esporta();
  CHECK(d.find("config progdata 'progdata'") != std::string::npos && d.find("option s1 'b:cam'") != std::string::npos && d.find("option s2 'e:logger'") != std::string::npos);
  cfgDefaults();
  CHECK(cfgImport(d.c_str(), d.size(), e));
  CHECK(cfg.dataSdN == 2 && cfg.dataSd[0].kind == 1 && !strcmp(cfg.dataSd[0].name, "cam") && cfg.dataSd[1].kind == 2 && !strcmp(cfg.dataSd[1].name, "logger"));
  CHECK(cfgImport(d.c_str(), d.size(), e) && cfg.dataSdN == 2);                                        // lo stesso testo di nuovo: sostituisce
  std::string dd = "config progdata 'progdata'\n\toption s1 's:script'\n\toption s2 'zz'\n\toption s3 'e:" + std::string(30, 'x') + "'\n\toption s4 'b:ok'\n";
  CHECK(cfgImport(dd.c_str(), dd.size(), e) && cfg.dataSdN == 1 && !strcmp(cfg.dataSd[0].name, "ok"));   // uno script non salva dati, nome lungo e voce storta ignorati
  std::string dbig = "config progdata 'progdata'\n";
  for (int i = 1; i <= 10; i++) dbig += "\toption s" + std::to_string(i) + " 'b:a" + std::to_string(i) + "'\n";
  CHECK(cfgImport(dbig.c_str(), dbig.size(), e) && cfg.dataSdN == VOS_BOOTPROG_MAX);
  CHECK(cfgImport(old.c_str(), old.size(), e) && cfg.dataSdN == VOS_BOOTPROG_MAX);                     // un file senza la sezione non la cancella
  cfgDefaults();
  CHECK(cfg.dataSdN == 0);
  return done("prova_cfgprog");
}
