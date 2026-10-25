// VesevOS - prove sul computer: aiuti di lettura senza String (core/vos_parse.h) confrontati con la String VERA di Arduino
#include <Arduino.h>
#include "vos_parse.h"
#include "mini.h"
#include <string>

// riferimento: cio che faceva il codice con String (trim e toInt veri del core)
static std::string refTrim(const char* s) { String t(s); t.trim(); return std::string(t.c_str()); }
static long refInt(const char* s) { return String(s).toInt(); }

int main() {
  const char* casi[] = {"", " ", "   \t\r\n", "a", " a", "a ", " a ", "\ta\t", "\r\na b\r\n", "\v\fx\f\v", "  due  parole  ", "x\ny", "\n\n\n", "ok\r"};
  for (const char* s : casi) {
    std::string_view v = trimView(s);
    CHECK(std::string(v) == refTrim(s));
  }
  const char* num[] = {"", " ", "0", "7", "-7", "+7", "  42", "\t42", "42abc", "abc", "-", "+", "1 2", "12|3", "00012", "2147483647", "2147483648", "-2147483648", "-2147483649",
                       "99999999999999999999", "  -0", "3.9", "0x10", "1e3", " \n 5"};
  for (const char* s : num) CHECK(viewToLong(s) == refInt(s));
  // una vista dentro di un testo piu lungo: si legge solo la parte vista (mesh 1 chiave: il ruolo e "1")
  { std::string_view r = "1 deadbeef"; CHECK(viewToLong(r.substr(0, 1)) == 1); CHECK(viewToLong(r) == 1); }
  { std::string_view r = "12345"; CHECK(viewToLong(r.substr(0, 2)) == 12 && viewToLong(r.substr(2)) == 345); }   // la vista NON si ferma allo zero finale
  // oltre 31 caratteri: si legge l'inizio, il numero enorme satura come atol
  { std::string big(60, '9'); CHECK(viewToLong(big) == refInt(big.c_str())); }
  // trim non cambia i caratteri interni e lavora su una vista senza zero finale
  { std::string_view r = "  ab  cd  "; CHECK(trimView(r.substr(0, 6)) == "ab"); CHECK(trimView(r) == "ab  cd"); }
  return done("parse");
}
