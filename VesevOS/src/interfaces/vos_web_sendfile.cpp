// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_sendfile.cpp
// Invia al browser un file della memoria interna (lingue, scaricamento dalla pagina File).
// Lo facevamo con PsychicFileResponse, che vuole il tipo FS di Arduino: qui si legge con VosFile.
#include "vos_web_int.h"
#include "../core/vos_risorse.h"

namespace webx {

static const size_t SEND_CHUNK = 8 * 1024;     // come la pagina di Psychic: file piccoli in un colpo solo, i grandi a pezzi

// Da "/lang/en.vld" ricava "en.vld" (il nome proposto al browser).
static const char* baseName(const char* path) {
  const char* slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}

esp_err_t sendFile(Res* s, const char* fsPath, const char* contentType, bool download, size_t salta) {
  VosFile f = drvFsOpen(fsPath, "r");
  if (!f || f.isDirectory()) return notFound(s);
  size_t size = f.size();
  if (salta) {                                       // si saltano i primi byte (intestazione di un file .vgz)
    if (salta >= size || !f.seek(salta)) return notFound(s);
    size -= salta;
  }

  char disposition[96 + FS_PATH_MAX];
  snprintf(disposition, sizeof disposition, "%s; filename=\"%s\"", download ? "attachment" : "inline", baseName(fsPath));
  s->setContentType(contentType);
  s->setContentLength(size);
  s->addHeader("Content-Disposition", disposition);

  size_t bufSize = size < SEND_CHUNK ? size : SEND_CHUNK;
  uint8_t* buf = (uint8_t*)malloc(bufSize ? bufSize : 1);
  if (!buf) return ko(s, tr("Memoria insufficiente"));

  esp_err_t err = ESP_OK;
  if (size < SEND_CHUNK) {
    size_t got = f.read(buf, size);
    s->setContent(buf, got);
    err = s->send();
  } else {
    s->sendHeaders();
    size_t got;
    while ((got = f.read(buf, SEND_CHUNK)) > 0) {
      err = s->sendChunk(buf, got);
      if (err != ESP_OK) break;
    }
    if (err == ESP_OK) s->finishChunking();
  }
  free(buf);
  return err;
}

// Un file singolo di /usr/share/vesevos (dati comuni, licenze minori): lo controlla SHA256SUMS la prima volta, poi si invia.
// false = manca o non e valido: la risposta non e stata toccata e il ripiego lo sceglie chi chiama. true = inviato, err e l'esito dell'invio.
bool sendRisFile(Res* s, RisFile rf, const char* contentType, bool gzip, const char* cache, esp_err_t& err) {
  VosFile f;
  size_t len = 0;
  RisEsito e = risFileApri(rf, f, len);
  uint8_t marchio[4] = {0, 0, 0, 0};
  bool vosr = f.read(marchio, sizeof marchio) == sizeof marchio && memcmp(marchio, "VOSR", 4) == 0;   // .vgz: i dati gzip stanno dopo l'intestazione
  f.close();                                         // il controllo e fatto e ricordato: sendFile riapre il file
  if (e != RIS_OK) {
    vlogl(LG_DBG, "RISORSE: %s non disponibile (%s)", risFileNome(rf), risCodice(e));
    return false;
  }
  char path[FS_PATH_MAX + 1];
  if (!risFilePercorso(rf, path, sizeof path)) return false;
  if (gzip) s->addHeader("Content-Encoding", "gzip");
  if (cache) s->addHeader("Cache-Control", cache);
  err = sendFile(s, path, contentType, false, vosr ? RIS_INTEST : 0);
  return true;
}

}  // namespace webx
