// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_nvs.h
// STRATO 1 (driver): unico posto che usa la NVS (nvs_flash di ESP-IDF). Il resto del firmware usa DrvNvs.
// Stessi nomi e stesso formato di Preferences (namespace, chiavi, tipi), cosi i dati gia salvati restano leggibili.
// Si chiude da sola alla fine del blocco. L'implementazione e in vos_drv_nvs.cpp.
#pragma once
#include <stdint.h>
#include <stddef.h>

class DrvNvs {
public:
  DrvNvs() = default;
  ~DrvNvs() { end(); }
  DrvNvs(const DrvNvs&) = delete;
  DrvNvs& operator=(const DrvNvs&) = delete;

  bool     begin(const char* ns, bool readOnly = false);
  void     end();
  uint32_t getUInt(const char* k, uint32_t d = 0);
  void     putUInt(const char* k, uint32_t v);
  uint8_t  getUChar(const char* k, uint8_t d = 0);
  void     putUChar(const char* k, uint8_t v);
  bool     getBool(const char* k, bool d = false);
  void     putBool(const char* k, bool v);
  // Legge un testo in un buffer: ritorna i caratteri letti (0 se manca o non ci sta; il buffer resta com'era)
  size_t   getString(const char* k, char* out, size_t cap);
  void     putString(const char* k, const char* v);
  // Dati binari di lunghezza fissa: getBlob e true solo se la chiave esiste e ha ESATTAMENTE n byte (altrimenti out resta com'era)
  bool     getBlob(const char* k, void* out, size_t n);
  void     putBlob(const char* k, const void* v, size_t n);
  bool     has(const char* k);               // la chiave esiste (di qualunque tipo)?
  void     remove(const char* k);
  void     clear();
private:
  uint32_t h_ = 0;           // nvs_handle_t
  bool     open_ = false;
  bool     ro_ = false;
};
