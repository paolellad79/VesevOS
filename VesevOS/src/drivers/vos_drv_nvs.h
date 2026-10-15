// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_nvs.h
// STRATO 1 (driver): unico posto che usa la NVS (Preferences). Il resto del firmware usa DrvNvs.
// Stessi nomi di Preferences, cosi il codice che la usa resta uguale. Si chiude da sola alla fine del blocco.
#pragma once
#include <Arduino.h>
#include <Preferences.h>

class DrvNvs {
public:
  bool     begin(const char* ns, bool readOnly = false) { return p_.begin(ns, readOnly); }
  void     end()                                        { p_.end(); }
  uint32_t getUInt(const char* k, uint32_t d = 0)       { return p_.getUInt(k, d); }
  void     putUInt(const char* k, uint32_t v)           { p_.putUInt(k, v); }
  uint8_t  getUChar(const char* k, uint8_t d = 0)       { return p_.getUChar(k, d); }
  void     putUChar(const char* k, uint8_t v)           { p_.putUChar(k, v); }
  bool     getBool(const char* k, bool d = false)       { return p_.getBool(k, d); }
  void     putBool(const char* k, bool v)               { p_.putBool(k, v); }
  String   getString(const char* k, const String& d = String()) { return p_.getString(k, d); }
  void     putString(const char* k, const String& v)    { p_.putString(k, v); }
  void     remove(const char* k)                        { p_.remove(k); }
  void     clear()                                      { p_.clear(); }
private:
  Preferences p_;     // composizione: il driver contiene Preferences, non la estende
};
