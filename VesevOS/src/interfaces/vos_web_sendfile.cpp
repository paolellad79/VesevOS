// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_web_sendfile.cpp
// Invia al browser un file della memoria interna (lingue, scaricamento dalla pagina File).
// Lo facevamo con PsychicFileResponse, che vuole il tipo FS di Arduino: qui si legge con VosFile.
#include "vos_web_int.h"

namespace webx {

static const size_t SEND_CHUNK = 8 * 1024;     // come la pagina di Psychic: file piccoli in un colpo solo, i grandi a pezzi

// Da "/lang/en.json" ricava "en.json" (il nome proposto al browser).
static const char* baseName(const char* path) {
  const char* slash = strrchr(path, '/');
  return slash ? slash + 1 : path;
}

esp_err_t sendFile(Res* s, const char* fsPath, const char* contentType, bool download) {
  VosFile f = drvFsOpen(fsPath, "r");
  if (!f || f.isDirectory()) return notFound(s);
  size_t size = f.size();

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

}  // namespace webx
