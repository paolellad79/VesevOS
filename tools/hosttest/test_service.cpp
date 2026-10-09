// VesevOS - prove sul computer: registro dei servizi (vos_service.cpp) con un servizio finto
#include <Arduino.h>
#include "vos_service.h"
#include "mini.h"

static int st = SVC_OFF, begins = 0, stops = 0, restarts = 0; static bool fail = false;
static int  f_state() { return st; }
static bool f_begin(String& err) { begins++; if (fail) { err = "no"; st = SVC_ERROR; return false; } st = SVC_ON; return true; }
static void f_stop() { stops++; st = SVC_OFF; }
static bool f_restart(String& err) { restarts++; st = SVC_ON; return true; }
static uint32_t f_ram() { return st == SVC_ON ? 1234 : 0; }
static const ServiceOps A = { "a", "Servizio A", "Prova", "a", 2, 2, f_state, f_begin, f_stop, nullptr, nullptr, nullptr, f_ram };
static const ServiceOps B = { "b", "Servizio B", "Prova", "b", 0, 1, f_state, f_begin, f_stop, f_restart, nullptr, nullptr, nullptr };
static const ServiceOps BAD = { "x", "Rotto", "Prova", "x", 0, 0, nullptr, f_begin, f_stop, nullptr, nullptr, nullptr, nullptr };
static const ServiceOps DUP = { "a", "Doppio", "Prova", "a", 0, 0, f_state, f_begin, f_stop, nullptr, nullptr, nullptr, nullptr };

int main() {
  String err;
  CHECK(serviceCount() == 0);
  CHECK(!serviceRegister(nullptr));
  CHECK(!serviceRegister(&BAD));                    // manca state
  CHECK(serviceRegister(&A));
  CHECK(!serviceRegister(&DUP));                    // id doppio
  CHECK(serviceRegister(&B));
  CHECK(serviceCount() == 2);
  CHECK(serviceAt(0) == &A && serviceAt(1) == &B && serviceAt(2) == nullptr && serviceAt(-1) == nullptr);
  CHECK(serviceFind("b") == &B && serviceFind("zz") == nullptr);
  CHECK(serviceState("a") == SVC_OFF && serviceState("zz") == SVC_OFF);
  CHECK(serviceStart("a", err) && serviceState("a") == SVC_ON && begins == 1);
  CHECK(serviceRam("a") == 1234 && serviceRam("zz") == 0);
  CHECK(serviceStop("a", err) && stops == 1 && serviceState("a") == SVC_OFF && serviceRam("a") == 0);
  CHECK(!serviceStart("zz", err) && err.length() > 0);
  CHECK(!serviceStop("zz", err));
  // riavvio: senza funzione propria = ferma poi avvia
  st = SVC_ON; begins = 0; stops = 0;
  CHECK(serviceRestart("a", err) && stops == 1 && begins == 1 && serviceState("a") == SVC_ON);
  // riavvio con funzione propria
  CHECK(serviceRestart("b", err) && restarts == 1);
  // avvio che fallisce: errore e stato "errore"
  fail = true; err = "";
  CHECK(!serviceStart("a", err) && err == "no" && serviceState("a") == SVC_ERROR);
  fail = false;
  // il registro ha un limite
  static ServiceOps many[VOS_MAX_SERVICES]; static char ids[VOS_MAX_SERVICES][8]; int added = 0;
  for (int i = 0; i < VOS_MAX_SERVICES; i++) { snprintf(ids[i], 8, "m%d", i); many[i] = A; many[i].id = ids[i]; if (serviceRegister(&many[i])) added++; }
  CHECK(added == VOS_MAX_SERVICES - 2 && serviceCount() == VOS_MAX_SERVICES);
  return done("service");
}
