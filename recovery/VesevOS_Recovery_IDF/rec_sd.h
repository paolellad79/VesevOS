// VesevOS Recovery - installa il firmware che VesevOS ha gia scaricato sulla microSD (ordine di modo 3).
#pragma once

#include <stdint.h>
#include <string>

struct RecSdFile {
  std::string file;                    // percorso sulla microSD
  uint8_t pin[4] = {0};                // CS, MOSI, SCK, MISO
  uint32_t khz = 4000;
  uint8_t sha[32] = {0};
  uint32_t size = 0;
};

bool recSdFileOk(const RecSdFile& f);  // percorso e pin ammessi (stesse regole dell'ordine)
// Controlla tutto il file, chiama primaDi (se c'e), poi scrive app0. false = motivo in code e why; cancellata dice se app0 e stata toccata.
bool recSdInstalla(const RecSdFile& f, bool togliFile, void (*primaDi)(), bool& cancellata, int& code, std::string& why);
void recSdRun();                       // se riesce riavvia e non torna; se torna, l'esito e scritto e l'ordine e chiuso
