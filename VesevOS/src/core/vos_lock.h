// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_lock.h
// Blocco breve tra task (mutex ricorsivo FreeRTOS) con chiusura automatica. Si crea con init() mentre si parte (un solo task), poi
// lock()/unlock() o VosLock su un blocco di codice. Dentro il blocco niente attese lunghe ne chiamate che prendono altri blocchi.
#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class VosMutex {
 public:
  void init() { if (!m_) m_ = xSemaphoreCreateRecursiveMutex(); }
  void lock() { if (m_) xSemaphoreTakeRecursive(m_, portMAX_DELAY); }
  void unlock() { if (m_) xSemaphoreGiveRecursive(m_); }
 private:
  SemaphoreHandle_t m_ = nullptr;
};

class VosLock {
 public:
  explicit VosLock(VosMutex& m) : m_(m) { m_.lock(); }
  ~VosLock() { m_.unlock(); }
  VosLock(const VosLock&) = delete;
  VosLock& operator=(const VosLock&) = delete;
 private:
  VosMutex& m_;
};
