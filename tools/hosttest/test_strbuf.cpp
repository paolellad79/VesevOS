// VesevOS - prove sul computer: scrittore su buffer fisso (core/vos_strbuf.h), licenze (vos_license.cpp), json del traffico
#include <Arduino.h>
#include "vos_strbuf.h"
#include "vos_license.h"
#include "mini.h"

int main() {
  // capacita giusta
  { char b[16]; StrBuf s(b, sizeof(b)); s.add("abc").add('-').addu(42);
    CHECK(strcmp(s.c_str(), "abc-42") == 0 && s.length() == 6 && !s.truncated() && s.required() == 6); }
  // buffer vuoto di testo
  { char b[4]; StrBuf s(b, sizeof(b)); CHECK(strcmp(s.c_str(), "") == 0 && s.length() == 0 && !s.truncated()); }
  // N-1: ci stanno 3 caratteri su capacita 4
  { char b[4]; StrBuf s(b, sizeof(b)); s.add("abc"); CHECK(strcmp(b, "abc") == 0 && !s.truncated() && s.required() == 3); }
  // N: il quarto non ci sta
  { char b[4]; StrBuf s(b, sizeof(b)); s.add("abcd"); CHECK(strcmp(b, "abc") == 0 && s.truncated() && s.required() == 4 && s.length() == 3); }
  // oltre la capacita, piu aggiunte: il testo resta chiuso con 0 e required conta tutto
  { char b[8]; StrBuf s(b, sizeof(b)); s.add("123456").add("7890").addu(12345); 
    CHECK(strcmp(b, "1234567") == 0 && s.truncated() && s.required() == 15); }
  // capacita 1 e capacita 0 (nessuna scrittura, ma si conta)
  { char b[1] = {'x'}; StrBuf s(b, 1); s.add("ab"); CHECK(b[0] == 0 && s.truncated() && s.required() == 2); }
  { StrBuf s(nullptr, 0); s.add("abc").addu(7).addf("%d", 99); CHECK(s.truncated() && s.required() == 6 && s.length() == 0 && strcmp(s.c_str(), "") == 0); }
  // numeri limite
  { char b[32]; StrBuf s(b, sizeof(b)); s.addu(0).add(',').addu(4294967295u); CHECK(strcmp(b, "0,4294967295") == 0); }
  // addf, anche con troncamento
  { char b[8]; StrBuf s(b, sizeof(b)); s.addf("%s=%d", "k", 5); CHECK(strcmp(b, "k=5") == 0 && !s.truncated());
    s.addf("%s", "0123456789"); CHECK(strcmp(b, "k=50123") == 0 && s.truncated() && s.required() == 13); }
  // UTF-8: lo scrittore taglia a byte (come snprintf): chi lo usa per testi utente deve saperlo. Qui si controlla solo che non esca dal buffer.
  { char b[4]; StrBuf s(b, sizeof(b)); s.add("\xc3\xa8\xc3\xa8"); CHECK(s.length() == 3 && s.truncated() && b[3] == 0); }
  // nullptr come testo = niente
  { char b[8]; StrBuf s(b, sizeof(b)); s.add((const char*)nullptr); CHECK(s.length() == 0 && !s.truncated()); }

  // StrText: un blocco, testo della lunghezza giusta, troncamento segnalato
  { StrText x(32); x.buf().add("ciao ").addu(42); std::string r = x.take(); CHECK(r == "ciao 42" && r.size() == 7); }
  { StrText x(6); x.buf().add("0123456789"); CHECK(x.truncated() && x.buf().required() == 10); std::string r = x.take(); CHECK(r == "01234"); }
  { StrText x(0); x.buf().add("a"); CHECK(x.truncated()); CHECK(x.take().empty()); }

  // licenze: ricerca (senza distinguere maiuscole), sconosciuta, nullptr
  CHECK(licFind("notice") == 0 && licFind("NOTICE") == 0 && licFind("Gpl3") >= 0);
  CHECK(licFind("non-esiste") == -1 && licFind("") == -1 && licFind(nullptr) == -1);
  for (int i = 0; i < licCount(); i++) CHECK(licFind(licId(i)) == i);
  // elenco JSON: sta nella capacita prevista (con margine), ed e uguale a come lo faceva la String
  { char j[LIC_JSON_MAX]; size_t n = licListJson(j, sizeof(j));
    CHECK(n == strlen(j) && n < LIC_JSON_MAX - 64);
    String atteso = "[";
    for (int i = 0; i < licCount(); i++) { if (i) atteso += ","; atteso += "{\"id\":\"" + String(licId(i)) + "\",\"title\":\"" + String(licTitle(i)) + "\",\"size\":" + String((unsigned)licSize(i)) + "}"; }
    atteso += "]";
    CHECK(atteso == j);
    char piccolo[20]; size_t m = licListJson(piccolo, sizeof(piccolo)); CHECK(m == 19 && strncmp(piccolo, j, 19) == 0 && piccolo[19] == 0); }
  { char ids[96]; size_t n = licIds(ids, sizeof(ids));
    String atteso; for (int i = 0; i < licCount(); i++) { if (i) atteso += ", "; atteso += licId(i); }
    CHECK(n == strlen(ids) && atteso == ids && n < sizeof(ids) - 8); }
  return done("strbuf");
}
