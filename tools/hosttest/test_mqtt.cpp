// VesevOS - prove sul computer: parti del client MQTT senza rete (packages/vos_mqtt_msg.h) confrontate con il codice di prima (String)
#include <Arduino.h>
#include "vos_util.h"
#include "vos_mqtt_msg.h"
#include "vos_mqtt_gate.h"
#include "mini.h"
#include <string>

// ---- riferimento: com'era il codice con String (vos_mqtt.cpp prima del gruppo 10) ----
static String refPrefix(const String& cfgPrefix, const String& host) {
  String p = cfgPrefix; p.trim();
  if (!p.length()) p = "vesevos/" + host;
  while (p.endsWith("/")) p.remove(p.length() - 1);
  return p;
}
static String refDevId(const String& mac) { String m = mac; m.replace(":", ""); m.toLowerCase(); return "vesevos_" + m; }
static String refCmd(const char* data, int len) {
  String p; p.reserve(len);
  for (int i = 0; i < len; i++) { char c = data[i]; p += (c >= 32 && c < 127) ? c : ' '; }
  return p;
}
static String refState(int cpu, int temp, unsigned long heap, unsigned long psram, int rssi, unsigned long up, const String& ip, const String& mac) {
  String j = "{";
  j += "\"cpu\":" + String(cpu);
  j += ",\"temp\":" + String(temp);
  j += ",\"heap\":" + String(heap);
  j += ",\"psram\":" + String(psram);
  j += ",\"rssi\":" + String(rssi);
  j += ",\"uptime\":" + String(up);
  j += ",\"ip\":\"" + ip + "\"";
  j += ",\"mac\":\"" + mac + "\"";
  j += ",\"version\":\"" "1.7.66a" "\"";
  return j + "}";
}
struct S { const char* k; const char* n; const char* unit; const char* cls; };
static const S LIST[] = {
  {"temp", "Temperatura CPU", "\xC2\xB0" "C", "temperature"}, {"cpu", "CPU", "%", ""},
  {"rssi", "Segnale Wi-Fi", "dBm", "signal_strength"}, {"heap", "RAM libera", "B", "data_size"},
  {"uptime", "Acceso da", "s", "duration"},
};
static String refDev(const String& id, const String& host) { return "\"dev\":{\"ids\":[\"" + id + "\"],\"name\":\"" + jsonEscape(host) + "\",\"mdl\":\"" "ESP32-S3" "\",\"mf\":\"VesevOS\",\"sw\":\"" "1.7.66a" "\"}"; }
static String refAv(const String& pre) { return "\"avty_t\":\"" + pre + "/status\""; }
static String refSensor(const S& x, const String& id, const String& pre, const String& av, const String& dev) {
  String c = "{\"name\":\"" + String(x.n) + "\",\"uniq_id\":\"" + id + "_" + x.k + "\",\"stat_t\":\"" + pre + "/state\",\"val_tpl\":\"{{ value_json." + x.k + " }}\"";
  if (x.unit[0]) c += ",\"unit_of_meas\":\"" + String(x.unit) + "\"";
  if (x.cls[0]) c += ",\"dev_cla\":\"" + String(x.cls) + "\"";
  c += ",\"stat_cla\":\"measurement\"," + av + "," + dev + "}";
  return c;
}
static String refButton(const String& id, const String& pre, const String& av, const String& dev) {
  return "{\"name\":\"Riavvia\",\"uniq_id\":\"" + id + "_reboot\",\"cmd_t\":\"" + pre + "/cmd\",\"pl_prs\":\"reboot\",\"dev_cla\":\"restart\"," + av + "," + dev + "}";
}
static std::string joined(const MqttPrefix& p) { return std::string(p.a) + std::string(p.b); }

int main() {
  // ---- prefisso ----
  const char* cfgs[] = {"", " ", "  \t ", "a", " a ", "a/b", "a/b/", "a//", "///", " / ", "vesevos/x", "casa/sala/", "x y", "#", "\"q\""};
  const char* hosts[] = {"vesevos", "vesevos3", "a-b", "", "x/"};
  for (const char* c : cfgs) for (const char* h : hosts) {
    String ref = refPrefix(c, h);
    MqttPrefix p = mqttPrefixParts(std::string_view(c), std::string_view(h));
    CHECK(joined(p) == std::string(ref.c_str()));
    CHECK(p.size() == ref.length());
    char buf[200]; CHECK(mqttPrefixCopy(p, buf, sizeof buf) && ref == buf);
  }
  { MqttPrefix p = mqttPrefixParts("abcdefghij", "x"); char b[5]; CHECK(!mqttPrefixCopy(p, b, sizeof b) && b[4] == 0); CHECK(!mqttPrefixCopy(p, b, 0)); }
  // il prefisso predefinito non e un testo diverso dal nome della scheda
  { MqttPrefix p = mqttPrefixParts("", "vesevos3"); CHECK(joined(p) == "vesevos/vesevos3" && p.a == "vesevos/" && p.b == "vesevos3"); }
  // ---- comando in arrivo: ogni byte, in ogni posizione, come prima ----
  for (int c = 0; c < 256; c++) {
    char d[6] = {'a', (char)c, 'b', (char)c, 'z', 0}; char out[8];
    mqttCmdClean(d, 5, out);
    String ref = refCmd(d, 5);
    CHECK(strlen(out) == 5 && ref == out);
  }
  { char out[2]; mqttCmdClean("", 0, out); CHECK(out[0] == 0); }
  { std::string big(200, '\x01'); char out[210]; mqttCmdClean(big.data(), 200, out); CHECK(strlen(out) == 200 && out[0] == ' ' && out[199] == ' '); }
  // ---- identificativo della scheda ----
  const char* macs[] = {"AA:BB:CC:DD:EE:FF", "aa:bb:cc:dd:ee:ff", "00:11:22:33:44:55", "A1:b2:C3:d4:E5:f6", "", "ABC"};
  for (const char* m : macs) { char b[24]; StrBuf s(b, sizeof b); mqttDevId(m, s); CHECK(!s.truncated() && refDevId(m) == b); }
  { char b[24]; StrBuf s(b, sizeof b); mqttDevId("AA:BB:CC:DD:EE:FF", s); CHECK(s.length() == 20); }   // il blocco di MqttRt conta su 21 byte
  // ---- escape JSON: stesso testo di jsonEscape() per ogni byte ----
  { std::string all; for (int c = 1; c < 256; c++) all += (char)c;
    char b[1024]; StrBuf s(b, sizeof b); mqttJsonEsc(s, all);
    CHECK(!s.truncated() && String(b) == jsonEscape(String(all.c_str()))); }
  { char b[16]; StrBuf s(b, sizeof b); mqttJsonEsc(s, ""); CHECK(s.length() == 0); }
  // ---- JSON dello stato ----
  { int cpus[] = {0, 7, 100}, temps[] = {-5, 0, 64}, rssis[] = {-90, 0};
    for (int cpu : cpus) for (int t : temps) for (int r : rssis) {
      char b[1024]; StrBuf s(b, sizeof b);
      mqttStateJson(s, cpu, t, 116400u, 2013000u, r, 4294967295u, "192.168.1.38", "AA:BB:CC:DD:EE:FF", "1.7.66a");
      String ref = refState(cpu, t, 116400UL, 2013000UL, r, 4294967295UL, "192.168.1.38", "AA:BB:CC:DD:EE:FF");
      CHECK(!s.truncated() && ref == b);
    }
    char b[300]; StrBuf s(b, sizeof b);
    mqttStateJson(s, 100, -99, 4294967295u, 4294967295u, -100, 4294967295u, "255.255.255.255", "AA:BB:CC:DD:EE:FF", "1.7.66a-wdwin-extra");
    CHECK(!s.truncated() && s.length() < 256); }   // anche al massimo il testo resta sotto la meta del buffer da 1024
  // ---- Home Assistant: stessi messaggi di prima con un prefisso normale ----
  { const char* pres[] = {"vesevos/vesevos3", "casa/sala", "x"}; const char* hs[] = {"vesevos3", "a-b-c", "x"};
    for (const char* pre : pres) for (const char* host : hs) {
      MqttPrefix pv = mqttPrefixParts(pre, host);
      String preS = refPrefix(pre, host);
      String id = refDevId("AA:BB:CC:DD:EE:FF");
      char dv[352], av[208]; StrBuf dev(dv, sizeof dv), avb(av, sizeof av);
      mqttHaDev(dev, id.c_str(), host, "ESP32-S3", "1.7.66a"); mqttHaAv(avb, pv);
      String refD = refDev(id, host), refA = refAv(preS);
      CHECK(!dev.truncated() && !avb.truncated() && refD == dv && refA == av);
      for (const S& x : LIST) {
        char sc[1024]; StrBuf c(sc, sizeof sc);
        mqttHaSensor(c, id.c_str(), x.k, x.n, x.unit, x.cls, pv, av, dv);
        CHECK(!c.truncated() && refSensor(x, id, preS, refA, refD) == sc);
      }
      char sc[1024]; StrBuf c(sc, sizeof sc);
      mqttHaRebootButton(c, id.c_str(), pv, av, dv);
      CHECK(!c.truncated() && refButton(id, preS, refA, refD) == sc);
    } }
  // ---- i limiti dei buffer: caso peggiore per le schede (nome 32 caratteri, prefisso 60 come dalla pagina) ----
  { std::string host(32, 'h'), pre(60, 'p');
    MqttPrefix pv = mqttPrefixParts(pre, host);
    char dv[352], av[208], sc[1024]; StrBuf dev(dv, sizeof dv), avb(av, sizeof av), c(sc, sizeof sc);
    mqttHaDev(dev, "vesevos_aabbccddeeff", host, "ESP32-S3", "1.7.66a-wdwin"); mqttHaAv(avb, pv);
    mqttHaSensor(c, "vesevos_aabbccddeeff", "uptime", "Temperatura CPU", "\xC2\xB0" "C", "signal_strength", pv, av, dv);
    CHECK(!dev.truncated() && !avb.truncated() && !c.truncated()); }
  // prefisso di 60 virgolette (la pagina le ammette): si escono tutte e il JSON resta intero o si scarta, mai a meta
  { std::string pre(60, '"'); MqttPrefix pv = mqttPrefixParts(pre, "h");
    char dv[352], av[208], sc[1024]; StrBuf dev(dv, sizeof dv), avb(av, sizeof av), c(sc, sizeof sc);
    mqttHaDev(dev, "vesevos_aabbccddeeff", "h", "ESP32-S3", "1.7.66a"); mqttHaAv(avb, pv);
    mqttHaSensor(c, "vesevos_aabbccddeeff", "temp", "Temperatura CPU", "\xC2\xB0" "C", "temperature", pv, av, dv);
    CHECK(!avb.truncated() && !c.truncated() && std::string(av).find("\\\"\\\"") != std::string::npos); }
  // prefisso enorme (importato da file, senza controllo): av non entra, e lo si vede
  { std::string pre(400, 'p'); MqttPrefix pv = mqttPrefixParts(pre, "h"); char av[208]; StrBuf avb(av, sizeof av); mqttHaAv(avb, pv); CHECK(avb.truncated()); }
  // ---- porta della rete: il client si apre solo con la rete pronta, mai prima ----
  { MqttGate g; int starts = 0;
    for (int i = 0; i < 1000; i++) if (mqttGateStep(g, false, false) != MG_NONE) starts++;      // rete mai pronta (setup): mai start
    CHECK(starts == 0); }
  { MqttGate g; int tick = 0, startAt = -1;
    for (; tick < 20; tick++) if (mqttGateStep(g, true, false) == MG_START) { startAt = tick; break; }
    CHECK(startAt == (int)MQTT_GATE_UP_TICKS - 1); }                                            // 1 s di rete pronta, non prima
  { MqttGate g; for (int i = 0; i < 3; i++) mqttGateStep(g, true, false);
    CHECK(mqttGateStep(g, false, false) == MG_NONE);                                          // rete che balena: il conto riparte
    int s = 0; for (int i = 0; i < 4; i++) if (mqttGateStep(g, true, false) == MG_START) s++;
    CHECK(s == 0); }
  { MqttGate g; int stops = 0;
    for (int i = 0; i < 100; i++) mqttGateStep(g, true, true);                                 // client aperto, rete ok: niente
    for (int i = 0; i < (int)MQTT_GATE_DOWN_TICKS - 1; i++) if (mqttGateStep(g, false, true) == MG_STOP) stops++;
    CHECK(stops == 0);                                                                         // interruzione breve: resta aperto
    CHECK(mqttGateStep(g, false, true) == MG_STOP); }                                          // oltre 5 s: si chiude
  { MqttGate g; for (int i = 0; i < 100; i++) mqttGateStep(g, false, true);
    CHECK(mqttGateStep(g, false, false) == MG_NONE); }                                         // gia chiuso: nessun altro stop
  { MqttGate g; g.upTicks = 0xFFFF; g.downTicks = 0xFFFF; mqttGateStep(g, true, true); mqttGateStep(g, false, true); CHECK(g.upTicks == 0 && g.downTicks == 1); }
  // ---- azzeramento dei segreti ----
  { char s[32]; memset(s, 'x', sizeof s); mqttSecureZero(s, 31); bool z = true; for (int i = 0; i < 31; i++) z = z && s[i] == 0; CHECK(z && s[31] == 'x'); mqttSecureZero(s, 0); CHECK(s[31] == 'x'); }
  return done("mqtt");
}
