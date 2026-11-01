// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_elf_quar_nvs.cpp: la quarantena dei moduli ELF salvata nella NVS (spazio "elfq"). Vedi vos_elf_quar.h.
#include "vos_elf_quar.h"

#if VOS_WITH_ELF
#include "../drivers/vos_drv_nvs.h"

namespace {

class NvsStore : public VosElfQuarStore {
 public:
  bool get(const char* chiave, char* out, size_t cap) override {
    out[0] = 0;
    DrvNvs n;
    if (!n.begin("elfq", true)) return false;
    return n.getString(chiave, out, cap) > 0;
  }
  void put(const char* chiave, const char* valore) override {
    DrvNvs n;
    if (!n.begin("elfq", false)) return;
    if (valore[0]) n.putString(chiave, valore);
    else n.remove(chiave);
  }
};

NvsStore g_nvs;

}  // namespace

void vos_elf_quar_nvs_usa() { vos_elf_quar_usa(&g_nvs); }
#endif
