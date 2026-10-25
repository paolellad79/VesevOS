// VesevOS - prove sul computer: Event Bus (vos_eventbus.cpp)
#include <Arduino.h>
#include "vos_eventbus.h"
#include "mini.h"

static int got = 0, gotAll = 0; static int32_t lastVal = -1; static char lastTopic[16];
static void onSvc(const Event& e) { got++; lastVal = e.val; strcpy(lastTopic, e.topic); }
static void onAll(const Event& e) { gotAll++; }
static void onRec(const Event& e) { EventBus::publish("x.inner", "", 1); gotAll += 100; }   // gestore che pubblica: non deve ricorrere
// un altro task pubblica mentre il primo sta ancora servendo un evento: il suo evento NON deve sparire (errore fino alla 1.7.58)
static int g_t1, g_t2, gotB = 0;
static void onA(const Event& e) { if (strcmp(e.topic, "a") == 0) { g_stubTask = &g_t2; EventBus::publish("b"); g_stubTask = &g_t1; } }
static void onB(const Event& e) { if (strcmp(e.topic, "b") == 0) gotB++; }

int main() {
  EventBus::reset();
  CHECK(EventBus::lastSeq() == 0);
  Event ev[40]; uint32_t lost = 9;
  CHECK(EventBus::since(0, ev, 40, &lost) == 0 && lost == 0);
  EventBus::publish("a.b", "id1", 5);
  EventBus::publish("a.c", "id2", 6);
  CHECK(EventBus::lastSeq() == 2);
  int n = EventBus::since(0, ev, 40, &lost);
  CHECK(n == 2 && lost == 0 && ev[0].seq == 1 && ev[1].seq == 2 && String(ev[1].arg) == "id2" && ev[1].val == 6);
  CHECK(EventBus::since(1, ev, 40) == 1 && ev[0].seq == 2);
  CHECK(EventBus::since(2, ev, 40) == 0);
  CHECK(EventBus::since(0, ev, 1) == 1 && ev[0].seq == 1);               // tetto max
  EventBus::publish("", "x");                                              // topic vuoto: ignorato
  EventBus::publish(nullptr);
  CHECK(EventBus::lastSeq() == 2);
  // testo pericoloso: niente virgolette, backslash o controlli nel JSON
  EventBus::publish("t\"x", "a\\b\n\"c", 1);
  n = EventBus::since(2, ev, 5);
  CHECK(n == 1 && String(ev[0].topic) == "t_x" && String(ev[0].arg) == "a_b__c");
  // troncamento
  EventBus::publish("0123456789abcdefghij", "0123456789012345678901234567890", 0);
  n = EventBus::since(3, ev, 5);
  CHECK(strlen(ev[0].topic) == 15 && strlen(ev[0].arg) == 19);
  // anello: sovrascrive i piu vecchi e dice quanti persi
  EventBus::reset();
  for (int i = 1; i <= 30; i++) EventBus::publish("r", "", i);
  CHECK(EventBus::lastSeq() == 30);
  n = EventBus::since(0, ev, 40, &lost);
  CHECK(n == EventBus::CAP && lost == 6 && ev[0].seq == 7 && ev[0].val == 7 && ev[n - 1].seq == 30);
  n = EventBus::since(25, ev, 40, &lost);
  CHECK(n == 5 && lost == 0 && ev[0].seq == 26);
  n = EventBus::since(500, ev, 40, &lost);                                 // numero dal futuro (riavvio): riparte
  CHECK(n == EventBus::CAP && ev[0].seq == 7);
  // JSON
  EventBus::reset();
  EventBus::publish("svc.state", "ble", 1);
  std::string j = EventBus::json(0);
  CHECK(j == "{\"last\":1,\"lost\":0,\"ev\":[{\"n\":1,\"ms\":0,\"t\":\"svc.state\",\"a\":\"ble\",\"v\":1}]}" || j.find("\"t\":\"svc.state\",\"a\":\"ble\",\"v\":1}]}") != std::string::npos);
  CHECK(EventBus::json(1) == "{\"last\":1,\"lost\":0,\"ev\":[]}");
  // tutti gli eventi dell'anello con valori estremi: il JSON entra nel blocco calcolato (mai tagliato), valido e con CAP voci
  EventBus::reset();
  for (int i = 0; i < 45; i++) EventBus::publish("svc.state.xxxxx", "abcdefghijklmnopqrs", (i & 1) ? -2147483647 - 1 : 2147483647);
  { std::string big = EventBus::json(0, 100); CHECK(!big.empty() && big.back() == '}' && big.size() > 2000);
    size_t cnt = 0, pos = 0; while ((pos = big.find("{\"n\":", pos)) != std::string::npos) { cnt++; pos++; } CHECK(cnt == (size_t)EventBus::CAP); }
  EventBus::reset();
  // ascoltatori
  EventBus::reset(); got = gotAll = 0;
  CHECK(EventBus::subscribe("svc.", onSvc));
  CHECK(!EventBus::subscribe("svc.", onSvc));                              // doppia
  CHECK(EventBus::subscribe("", onAll));
  CHECK(!EventBus::subscribe("x", nullptr));
  EventBus::publish("svc.state", "ble", 3);
  EventBus::publish("net.up", "", 1);
  CHECK(got == 1 && gotAll == 2 && lastVal == 3 && String(lastTopic) == "svc.state");
  CHECK(EventBus::unsubscribe(onSvc) && !EventBus::unsubscribe(onSvc));
  EventBus::publish("svc.state", "ble", 0);
  CHECK(got == 1 && gotAll == 3);
  // tetto ascoltatori
  EventBus::reset();
  char p[8]; int okc = 0;
  for (int i = 0; i < 12; i++) { snprintf(p, sizeof(p), "p%d", i); if (EventBus::subscribe(p, onAll)) okc++; }
  CHECK(okc == EventBus::MAXSUB);
  // anti-ricorsione
  EventBus::reset(); gotAll = 0;
  CHECK(EventBus::subscribe("", onRec));
  EventBus::publish("go");
  CHECK(gotAll == 100 && EventBus::lastSeq() == 2);                        // l'evento interno e registrato, ma non richiama il gestore
  // due task: l'evento del secondo arriva ai gestori; lo stesso task resta protetto dalla ricorsione
  EventBus::reset(); gotB = 0; g_stubTask = &g_t1;
  CHECK(EventBus::subscribe("a", onA) && EventBus::subscribe("b", onB));
  EventBus::publish("a");
  CHECK(gotB == 1);
  EventBus::publish("b");
  CHECK(gotB == 2);                                                        // il segno del task si libera alla fine
  g_stubTask = nullptr;
  CHECK(EventBus::ramBytes() > 0 && EventBus::ramBytes() < 2200);
  return done("eventbus");
}
