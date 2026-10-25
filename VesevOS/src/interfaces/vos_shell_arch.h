// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_arch.h
// INTERNO ai comandi di compressione e archivi (gzip, gunzip, zcat, unzip, zip, tar): limiti, protezioni e pezzi in comune.
// PROTEZIONI (valgono per tutti):
//  - zip slip: un nome di una voce con '/' iniziale, '..', '\' o caratteri strani rifiuta TUTTO l'archivio prima di scrivere qualcosa;
//    il percorso finale passa da shellResolvePath e deve restare dentro la cartella di destinazione (mai la radice "/")
//  - file protetti (/vesevos.conf, /audit.log, /crash.txt): non si leggono, non si scrivono, non si sovrascrivono
//  - zip e tar -c non mettono MAI in un archivio i file protetti (un archivio non deve diventare un modo per portare fuori i segreti)
//  - zip bomb: al massimo ARCH_MAX_ENTRIES voci, ARCH_MAX_ENTRY byte per voce, ARCH_MAX_TOTAL byte in tutto (e mai oltre lo spazio libero),
//    rapporto di espansione al massimo ARCH_MAX_RATIO a 1 (sopra ARCH_RATIO_FROM byte espansi)
//  - scrittura con temporaneo e scambio (FsAtomicWriter): un errore a meta non lascia mezzi file
//  - una sola operazione alla volta, tetto di tempo, cessione della CPU, Ctrl+C dalla seriale (DrvDefGuard)
#pragma once
#include "vos_shell_int.h"
#include "../core/vos_files_stream.h"
#include "../drivers/vos_drv_deflate.h"

namespace shx {

const size_t   ARCH_MAX_ENTRY   = 1048576;   // byte di una voce estratta
const size_t   ARCH_MAX_TOTAL   = 2097152;   // byte espansi in tutto per comando
const unsigned ARCH_MAX_ENTRIES = 64;        // voci di un archivio
const unsigned ARCH_MAX_RATIO   = 100;       // espansione massima (espansi : compressi)
const size_t   ARCH_RATIO_FROM  = 1024;      // il rapporto si controlla solo per voci espanse piu di cosi
const size_t   ARCH_TEXT_MAX    = 16384;     // gzip -c e zcat: testo a schermo al massimo (poi si rifiuta)
const size_t   ARCH_NAME_MAX    = 100;       // caratteri del nome di una voce (stesso tetto di un percorso)
const size_t   ARCH_SPACE_KEEP  = 16384;     // spazio libero da lasciare sempre
const size_t   ARCH_IO_BUF      = 2048;      // buffer di lavoro per la lettura dei file
const unsigned ARCH_DEPTH_MAX   = 2;         // zip -r: cartelle dentro cartelle che si scendono (3 livelli in tutto)
const size_t   ARCH_KIDS_MAX    = FS_DIR_MAX + 4;   // voci di una cartella che si accettano (il tetto di LittleFS e 20; un po' di margine per le cartelle vecchie)

// Limiti che dipendono da chi scrive: la pagina web gira nel task httpd (stack 8 KB, watchdog 30 s) e serve tutti con un solo task: tetto basso.
struct ArchLimits {
  uint32_t maxMs;      // tempo massimo del comando
  size_t   maxIn;      // file (o archivio) di ingresso, in byte
  bool     serial;     // Ctrl+C attivo
};
ArchLimits archLimits(uint32_t sess);                 // seriale: 120 s e 1 MB (Ctrl+C ferma); pagina web, script e regole: 10 s e 128 KB

// Contesto di un comando. begin() prende il turno (una sola operazione alla volta) e fa partire il tempo.
class ArchCtx {
 public:
  ArchCtx(const char* cmd, OutSink& o, uint32_t sess);
  bool begin();                                       // false = occupato (il messaggio e gia scritto)
  OutSink&          out() { return o_; }
  const char*       cmd() const { return cmd_; }
  const ArchLimits& lim() const { return lim_; }
  bool              stopped() const { return stopped_; }   // Ctrl+C o tempo scaduto: non si continua con altri file
  DrvDefGuard&      guard() { return guard_; }
  // "cmd: nome: motivo" (nome puo essere "")
  void fail(const char* name, const char* why);
  // Errore del driver (o del sink: sinkWhy e il motivo se e = DDF_SINK). Per tempo scaduto e file troppo grandi aggiunge il consiglio per la web.
  void failDef(const char* name, DrvDefErr e, const char* sinkWhy);
  void tooBig(const char* name, size_t max);          // "troppo grande (massimo N KB)" + consiglio
  // Spazio per i dati espansi: min(ARCH_MAX_TOTAL, libero - ARCH_SPACE_KEEP), 0 se non basta
  static size_t budget();
 private:
  void webHint();
  const char* cmd_;
  OutSink&    o_;
  ArchLimits  lim_;
  DrvDefGuard guard_;
  DrvDefLock  lock_;
  bool        stopped_ = false;
};

// Ctrl+C dalla seriale (codice 3): per DrvDefGuard
bool archSerialCancel(void* unused);

// Nome di una voce di archivio in forma sicura. raw non ha lo zero finale (len byte). Ritorna false se il nome e pericoloso:
// vuoto, piu lungo di ARCH_NAME_MAX, '/' iniziale, '\', caratteri non ASCII o di controllo, "..", "//". "./" e "." spariscono.
// out: nome relativo senza '/' iniziale e senza '/' finale (vuoto = voce da ignorare); isDir: il nome finiva con '/'.
bool archSafeName(const char* raw, size_t len, char* out, size_t cap, bool* isDir);
// Percorso finale = base + nome, via shellResolvePath. Rifiuta se esce da base (o se base e la radice).
bool archTarget(const char* base, const char* rel, char* out);   // out: almeno FS_PATH_MAX + 1

// Percorso di un file dentro un archivio che si CREA: base e la cartella di riferimento. Fuori da base si toglie la '/' iniziale (stripped = true).
// Ritorna "" se abs e base stessa.
const char* archRelName(const char* base, const char* abs, bool& stripped);

// File che non devono mai finire in un archivio: i protetti (fsProtected) e quelli che portano chiavi, certificati o copie di configurazione
// (estensioni .pem .key .crt .der .p12 .vbk, senza badare alle maiuscole). path: percorso pulito.
bool archIsSecret(const char* path);
// Nome nascosto: comincia con '.' oppure e un temporaneo ".tmp~". name: solo il nome, senza cartelle.
bool archIsHidden(const char* name);

// I nomi dei figli di una cartella, letti in una volta e poi la cartella si chiude (cosi non restano aperte piu cartelle insieme).
struct ArchKids {
  char     name[ARCH_KIDS_MAX][ARCH_NAME_MAX + 1];
  unsigned count;
};
bool archReadKids(VosFile& dir, ArchKids& k);        // false se la cartella ha piu di ARCH_KIDS_MAX voci. k va azzerato (ArchHeap lo fa)

// Data e ora in formato DOS (zip): l'ora di sistema se e valida (NTP sincronizzato o impostata a mano), altrimenti 1980-01-01 00:00.
// LittleFS non salva la data dei file: e sempre l'ora della creazione dell'archivio, uguale per tutte le voci.
void archDosStamp(uint16_t* dosTime, uint16_t* dosDate);

// Numeri little endian nei formati binari (zip)
inline void archPutLe16(uint8_t* p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
inline void archPutLe32(uint8_t* p, uint32_t v) { archPutLe16(p, v); archPutLe16(p + 2, v >> 16); }

// Il nome di una voce e uno dei filtri della riga (le parole da firstFilter in poi, nomi esatti o cartelle) o sta dentro una cartella filtrata?
// firstFilter = 0: nessun filtro, vale tutto.
bool archSelected(const char* line, int firstFilter, const char* name);

// Un file in uscita: FsAtomicWriter + conteggio dei byte e CRC32. declared = byte che deve avere (SIZE_MAX = non si sa).
class ArchFileOut {
 public:
  bool begin(const char* path, bool overwrite, size_t declared, size_t maxBytes, ErrBuf& err);
  bool put(const uint8_t* data, size_t n);            // false se oltre il dichiarato, il tetto o lo spazio
  static bool sink(const uint8_t* data, size_t n, void* self) { return static_cast<ArchFileOut*>(self)->put(data, n); }
  bool finish(ErrBuf& err);                           // controlla i byte e scambia il file
  void abort() { w_.abort(); }
  bool patch(size_t pos, const uint8_t* data, size_t n) { return w_.patch(pos, data, n); }   // riscrive byte gia scritti (poi si riparte dalla fine)
  const char* why() const;                            // perche put e fallita
  uint32_t crc() const { return crc_; }
  size_t   bytes() const { return n_; }
 private:
  FsAtomicWriter w_;
  size_t   declared_ = 0;
  size_t   n_ = 0;
  uint32_t crc_ = 0;
  bool     over_ = false;
};

// Buffer di lavoro per leggere i file (sull'heap, con ArchHeap)
struct ArchIo { uint8_t b[ARCH_IO_BUF]; };

// Parole e opzioni: l'argomento w e un'opzione breve ("-abc")?
inline bool isShortOpt(const Word& w) { return w.length() > 1 && w.c_str()[0] == '-' && w.c_str()[1] != '-'; }

}  // namespace shx

// Un blocco di memoria sull'heap, azzerato, che si libera da solo (buffer di lavoro dei comandi: lo stack del server web e piccolo).
// Solo per dati semplici (numeri, array, ErrBuf): non chiama costruttori.
template <typename T>
class ArchHeap {
 public:
  ArchHeap() : p_(static_cast<T*>(calloc(1, sizeof(T)))) {}
  ~ArchHeap() { free(p_); }
  ArchHeap(const ArchHeap&) = delete;
  ArchHeap& operator=(const ArchHeap&) = delete;
  T* operator->() { return p_; }
  T& operator*() { return *p_; }
  explicit operator bool() const { return p_ != nullptr; }
 private:
  T* p_;
};
