#!/bin/bash
# Prova di FatFs (sorgente di ESP-IDF) con le STESSE opzioni di Arduino-ESP32 3.3.12 (nomi lunghi su stack, 255 caratteri, API in UTF-8, code page 850, exFAT spento)
# su un disco in RAM: nomi lunghi, spazi, maiuscole/minuscole, UTF-8, caratteri vietati. Non e la libreria SD della scheda (non prova l'SPI), ma lo stesso file system.
# Uso: tools/fatfs_lfn/prova.sh   (serve un compilatore C e il sorgente di ESP-IDF in $IDF_PATH, di fabbrica ~/esp/esp-idf)
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
F="${IDF_PATH:-$HOME/esp/esp-idf}/components/fatfs/src"
[ -f "$F/ff.c" ] || { echo "KO: non trovo $F/ff.c (imposta IDF_PATH)"; exit 1; }
OUT="${TMPDIR:-/tmp}/vos_fatfs_lfn"; mkdir -p "$OUT"
cc -std=gnu11 -w -I"$HERE" -I"$F" -DCONFIG_FATFS_LFN_STACK=1 -DCONFIG_FATFS_MAX_LFN=255 -DCONFIG_FATFS_API_ENCODING_UTF_8=1 -DCONFIG_FATFS_CODEPAGE=850 \
   -DCONFIG_WL_SECTOR_SIZE=512 -DCONFIG_FATFS_VOLUME_COUNT=1 -DCONFIG_FATFS_USE_FASTSEEK=0 -DCONFIG_FATFS_USE_LABEL=0 -DCONFIG_FATFS_PER_FILE_CACHE=1 \
   -DCONFIG_FATFS_DONT_TRUST_FREE_CLUSTER_CNT=0 -DCONFIG_FATFS_DONT_TRUST_LAST_ALLOC=0 -DCONFIG_FATFS_FS_LOCK=0 -DCONFIG_FATFS_TIMEOUT_MS=10000 \
   -DportTICK_PERIOD_MS=1 -DCONFIG_FATFS_USE_DYN_BUFFERS=0 "$HERE/fftest.c" "$F/ff.c" "$F/ffunicode.c" -o "$OUT/fftest"
"$OUT/fftest"
