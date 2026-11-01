// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_sdgate.cpp (vedi vos_shell_sdgate.h)
#include "vos_shell_sdgate.h"
#include "../core/vos_i18n.h"
#include "../core/vos_paths.h"
#include "../devices/vos_sd.h"

void SdHold::take() {
#if VOS_WITH_SD
  if (!held_) { sdOpEnter(); held_ = true; }
#endif
}

void SdHold::release() {
#if VOS_WITH_SD
  if (held_) { sdOpLeave(); held_ = false; }
#endif
}

bool shellSdReady(const char* path, const char* shown, OutSink& o, SdHold* hold) {
  if (!pathSdRel(path)) return true;
#if VOS_WITH_SD
  if (sdFormatting()) { o.println(trf("%s: %s", shown, sdBusyText())); return false; }
  if (!sdUp()) { o.println(trf("%s: %s", shown, tr("microSD non montata"))); return false; }
  if (hold) hold->take();
  return true;
#else
  o.println(trf("%s: %s", shown, tr("microSD non montata")));
  return false;
#endif
}
