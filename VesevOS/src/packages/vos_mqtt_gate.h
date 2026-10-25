// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_mqtt_gate.h
// Decide QUANDO il client MQTT puo vivere: solo con la rete di casa pronta (indirizzo IP ottenuto).
// Prima della rete lo stack lwIP non e pronto: esp-mqtt chiamerebbe getaddrinfo e il sistema andrebbe in panic (boot loop).
// Una funzione pura, senza Arduino: si prova sul computer (tools/hosttest/test_mqtt.cpp). Viene chiamata ogni 200 ms dal task "mqtt".
#pragma once
#include <stdint.h>

enum MqttGateAct : uint8_t { MG_NONE = 0, MG_START, MG_STOP };

struct MqttGate {
  uint16_t upTicks = 0, downTicks = 0;      // da quanti giri la rete e pronta / non e pronta
};

constexpr uint16_t MQTT_GATE_UP_TICKS = 5;        // 1 s di rete pronta prima di aprire il client (l'indirizzo si assesta)
constexpr uint16_t MQTT_GATE_DOWN_TICKS = 25;     // 5 s di rete assente prima di chiudere (un'interruzione breve non chiude nulla)

// netUp = rete di casa con indirizzo; open = il client esiste adesso.
inline MqttGateAct mqttGateStep(MqttGate& g, bool netUp, bool open) {
  if (netUp) {
    g.downTicks = 0;
    if (g.upTicks < 0xFFFF) g.upTicks++;
    return (!open && g.upTicks >= MQTT_GATE_UP_TICKS) ? MG_START : MG_NONE;
  }
  g.upTicks = 0;
  if (g.downTicks < 0xFFFF) g.downTicks++;
  return (open && g.downTicks >= MQTT_GATE_DOWN_TICKS) ? MG_STOP : MG_NONE;
}
