// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_shell_path.cpp
// La risoluzione vera sta in core/vos_paths (una sola per shell, Berry e API dei file). Qui resta il nome storico usato dalla shell.
#include "vos_shell_path.h"
#include "../core/vos_pathres.h"

bool shellResolvePath(const char* cwd, const char* arg, char* out, size_t cap) {
  return pathResolve(cwd, arg, out, cap);
}
