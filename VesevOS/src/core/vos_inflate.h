// VesevOS - vos_inflate.h: decompressore gzip minimo (nostro, ~150 righe). Serve per il dizionario inglese compresso nel firmware.
// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-VesevOS-Commercial
#pragma once
#include <stddef.h>
#include <stdint.h>
// Dimensione del testo non compresso (ultimi 4 byte del gzip), 0 se il file non e valido
size_t gzipRawSize(const uint8_t* gz, size_t n);
// Decomprime un file gzip in dst (capienza cap). Ritorna i byte scritti, 0 se errore.
size_t gzipUnpack(const uint8_t* gz, size_t n, uint8_t* dst, size_t cap);
