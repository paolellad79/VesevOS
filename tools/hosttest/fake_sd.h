#pragma once
// microSD finta per le prove sul computer (backend "host" del contratto drivers/vos_drv_sd.h):
// file e cartelle in memoria, scheda presente/assente/tolta a comando, errori a comando.
#include <map>
#include <set>
#include <string>

struct FakeSd {
  bool card;                 // c'e una scheda FAT nello slot
  bool gone;                 // la scheda e stata tolta mentre era montata
  bool failWrite;            // le scritture falliscono (scheda protetta o piena)
  bool corrupt;              // la prova rilegge dati diversi
  bool up;
  bool exfat;                // la scheda c'e ma e exFAT
  bool badfs;                // la scheda c'e ma non e FAT (settore valido)
  bool noResponse;           // il modulo risponde ma i dati non sono validi (MISO scollegato)
  int fs;                    // DrvSdFs del volume (3 = FAT32)
  int lastFail;              // DrvSdFail dell'ultimo drvSdBegin
  int begins;
  int ends;
  int writes;
  int reads;
  int proves;
  int cs;
  int mosi;
  int sck;
  int miso;
  unsigned hz;
  std::map<std::string, std::string> files;
  std::set<std::string> dirs;
};

extern FakeSd g_sd;

void fakeSdReset();

// Situazioni pronte per il test di contratto (test_sd_contract.cpp): ogni backend "host" ne offre l'equivalente.
enum FakeSdScenario {
  FSC_CARD_OK,        // scheda FAT32 inserita
  FSC_NO_CARD,        // slot vuoto
  FSC_EXFAT,          // scheda exFAT
  FSC_BAD_FS,         // scheda con settore valido ma non FAT
  FSC_NO_RESPONSE,    // modulo che risponde con dati non validi
  FSC_CARD_REMOVED,   // la scheda viene tolta mentre e montata
  FSC_WRITE_FAILS     // la scheda c'e ma le scritture falliscono
};

void fakeSdScenario(FakeSdScenario scenario);
