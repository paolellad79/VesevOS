// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_drv_fs.cpp
// Il file system interno: esp_littlefs (montaggio, formato, spazio) + POSIX/VFS (file e cartelle).
#include "vos_drv_fs.h"
#include "vos_drv_fs_cache.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <esp_littlefs.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#ifndef VOS_FS_MOUNT
#define VOS_FS_MOUNT "/littlefs"               // sul computer (prove) si cambia con -DVOS_FS_MOUNT="\"cartella\"": una cartella vera
#endif
static const char* FS_MOUNT = VOS_FS_MOUNT;    // dove si monta
static const char* FS_LABEL = "spiffs";        // nome della partizione (tabella partizioni di VesevOS)
static const size_t FS_FULL_MAX = 192;         // "/littlefs" + percorso pulito (max 100) con margine

static FsUsedCache g_used;   // spazio usato ricordato: vedi vos_drv_fs_cache.h

bool drvFsVfsPath(const char* path, char* out, size_t outSize) {
  if (!path || path[0] != '/') return false;
  int n = snprintf(out, outSize, "%s%s", FS_MOUNT, path);
  return n > 0 && (size_t)n < outSize;
}

bool drvFsBegin(bool formatOnFail) {
  if (esp_littlefs_mounted(FS_LABEL)) return true;
  esp_vfs_littlefs_conf_t conf = {};
  conf.base_path = FS_MOUNT;
  conf.partition_label = FS_LABEL;
  conf.format_if_mount_failed = false;       // il formato lo decidiamo noi sotto, solo se serve
  conf.grow_on_mount = true;                 // come faceva Arduino
  esp_err_t err = esp_vfs_littlefs_register(&conf);
  if (err == ESP_FAIL && formatOnFail && esp_littlefs_format(FS_LABEL) == ESP_OK) err = esp_vfs_littlefs_register(&conf);
  return err == ESP_OK;
}

VosFile drvFsOpen(const char* path, const char* mode) {
  char full[FS_FULL_MAX];
  if (!mode) mode = "r";
  if (!drvFsVfsPath(path, full, sizeof full)) return VosFile();
  if (mode[0] != 'r') g_used.touch();
  return VosFile::openPath(full, path, mode);
}

bool drvFsExists(const char* path) {
  char full[FS_FULL_MAX];
  struct stat st;
  return drvFsVfsPath(path, full, sizeof full) && stat(full, &st) == 0;
}

bool drvFsRemove(const char* path) {
  char full[FS_FULL_MAX];
  g_used.touch();
  return drvFsVfsPath(path, full, sizeof full) && unlink(full) == 0;
}

bool drvFsRename(const char* from, const char* to) {
  char a[FS_FULL_MAX], b[FS_FULL_MAX];
  g_used.touch();
  return drvFsVfsPath(from, a, sizeof a) && drvFsVfsPath(to, b, sizeof b) && rename(a, b) == 0;
}

// Se la cartella esiste gia e' un successo (come prima); se il nome e' di un file, no.
bool drvFsMkdir(const char* path) {
  char full[FS_FULL_MAX];
  struct stat st;
  g_used.touch();
  if (!drvFsVfsPath(path, full, sizeof full)) return false;
  if (stat(full, &st) == 0) return S_ISDIR(st.st_mode);
  return mkdir(full, 0777) == 0;
}

bool drvFsRmdir(const char* path) {
  char full[FS_FULL_MAX];
  g_used.touch();
  return drvFsVfsPath(path, full, sizeof full) && rmdir(full) == 0;
}

size_t drvFsUsed() { return g_used.used; }   // l'ultimo valore ricordato: non scorre mai il file system (vedi drvFsUsedRefresh)
bool   drvFsUsedDue(uint32_t nowSec) { return g_used.due(nowSec); }

// Il calcolo vero. Si chiama da UN solo task (il controllo "monitor"), mai dal web server. Con tante voci puo durare piu di 5 s: per questo il
// task scende alla priorita del task idle (come lui, a turno), cosi l'idle gira e il Task watchdog non scatta.
void drvFsUsedRefresh(uint32_t nowSec) {
  TaskHandle_t me = xTaskGetCurrentTaskHandle();
  UBaseType_t prio = uxTaskPriorityGet(me);
  size_t total = 0, used = 0;
  vTaskPrioritySet(me, tskIDLE_PRIORITY);
  if (esp_littlefs_info(FS_LABEL, &total, &used) != ESP_OK) used = 0;
  vTaskPrioritySet(me, prio);
  g_used.store(used, nowSec);
}

size_t drvFsTotal() {
  size_t total = 0, used = 0;
  return esp_littlefs_info(FS_LABEL, &total, &used) == ESP_OK ? total : 0;
}
