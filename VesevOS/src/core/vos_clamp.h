// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_clamp.h
// Limita un valore tra un minimo e un massimo (al posto della macro constrain di Arduino).
#pragma once
#include <type_traits>

template <class T, class L, class H>
constexpr std::common_type_t<T, L, H> vosClamp(T v, L lo, H hi) {
  using R = std::common_type_t<T, L, H>;
  return (R)v < (R)lo ? (R)lo : (R)v > (R)hi ? (R)hi : (R)v;
}
