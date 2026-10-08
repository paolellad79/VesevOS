// VesevOS - prove sul computer: funzioni di utilita (vos_util.cpp)
#include <Arduino.h>
#include "vos_util.h"
#include "mini.h"

int main() {
  // indirizzi IP
  uint32_t ip = 0;
  CHECK(ipParse("192.168.1.5", ip) && ip == 0xC0A80105u);
  CHECK(!ipParse("256.1.1.1", ip));
  CHECK(!ipParse("1.2.3", ip));
  CHECK(!ipParse("1.2.3.4.5", ip));
  CHECK(!ipParse("a.b.c.d", ip));
  CHECK(!ipParse("", ip));
  CHECK(ipToStr(0xC0A80105u) == "192.168.1.5");
  // maschere
  CHECK(maskValid(0xFFFFFF00u));          // /24
  CHECK(maskValid(0xFF000000u));          // /8
  CHECK(!maskValid(0xFFFFFFFFu));         // /32 troppo lunga
  CHECK(!maskValid(0));
  CHECK(!maskValid(0xFF00FF00u));         // non continua
  // tempo acceso
  CHECK(uptimeStr(0) == "0s");
  CHECK(uptimeStr(59) == "59s");
  CHECK(uptimeStr(61) == "1min 1s");
  CHECK(uptimeStr(3600) == "1h 0min 0s");
  CHECK(uptimeStr(86400 + 3661) == "1g 1h 1min 1s");
  // testo pulito
  CHECK(cleanAscii("ciao\r\nmondo") == "ciao\nmondo");
  CHECK(cleanAscii("a\xC3\xA8" "b") == "a??b");
  CHECK(jsonEscape("a\"b\\c\n") == "a\\\"b\\\\c\\n");
  // SHA-256 (valori noti)
  CHECK(sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  CHECK(sha256Hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  // numeri casuali
  String r = randomHex(16);
  CHECK(r.length() == 32);
  CHECK(randomHex(16) != r);
  // nomi
  CHECK(hostnameValid("vesevos"));
  CHECK(hostnameValid("a-b"));
  CHECK(!hostnameValid("-ab"));
  CHECK(!hostnameValid("ab-"));
  CHECK(!hostnameValid("a b"));
  CHECK(!hostnameValid(""));
  CHECK(!hostnameValid("abcdefghijklmnopqrstuvwxyzabcdefg"));   // 33
  CHECK(domainValid(""));
  CHECK(domainValid("casa.local"));
  CHECK(!domainValid("casa..local"));
  CHECK(!domainValid(".local"));
  // F1: argomenti con virgolette (wifi set "Casa mia" password)
  { int p = 0; String a, b;
    CHECK(utilTakeArg("\"Casa mia\" segreta123", p, a) && a == "Casa mia");
    CHECK(utilTakeArg("\"Casa mia\" segreta123", p, b) && b == "segreta123");
    CHECK(!utilTakeArg("\"Casa mia\" segreta123", p, b));            // finito
    p = 0; CHECK(utilTakeArg("rete1   pw", p, a) && a == "rete1");     // senza virgolette come prima
    p = 0; CHECK(utilTakeArg("\"a\\\"b\"", p, a) && a == "a\"b");    // \" dentro = una virgoletta
    p = 0; CHECK(utilTakeArg("\"senza fine", p, a) && a == "senza fine");
    p = 0; CHECK(!utilTakeArg("   ", p, a));
    p = 0; CHECK(utilTakeArg("\"\"", p, a) && a == "");              // virgolette vuote = argomento vuoto
  }
  // nome di server NTP e controllo dell'ora ricevuta
  CHECK(utilHostOk("pool.ntp.org") && utilHostOk("0.it-pool.ntp.org") && utilHostOk("a"));
  CHECK(!utilHostOk("") && !utilHostOk("a b") && !utilHostOk("x;y") && !utilHostOk("a/b") && !utilHostOk(String("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
  CHECK(!utilTimeSyncOk(0, 0, 0) && !utilTimeSyncOk(1700000000UL, 0, 0));          // prima del 2026: mai
  CHECK(utilTimeSyncOk(1790000000UL, 0, 0));                                       // nessuna ora attesa: basta che sia dopo il 2026
  CHECK(utilTimeSyncOk(1790000100UL, 1790000000UL, 0));                            // scarto piccolo: ok
  CHECK(!utilTimeSyncOk(1790200000UL, 1790000000UL, 0));                           // salto di oltre un giorno: scartata
  CHECK(!utilTimeSyncOk(1789000000UL, 1790000000UL, 2));                           // anche all'indietro, alla terza volta ancora no
  CHECK(utilTimeSyncOk(1789000000UL, 1790000000UL, 3));                            // alla quarta si accetta: l'ora vera e cambiata
  CHECK(!utilTimeSyncOk(1700000000UL, 1790000000UL, 9));                           // ma prima del 2026 mai
  // stack dei task: quanto si puo togliere tenendo il 25% libero, a passi di 256 byte
  CHECK(utilStackSpare(0, 500) == 0);
  CHECK(utilStackSpare(4096, 1024) == 0);            // 1024 = esattamente il 25%: niente da togliere
  CHECK(utilStackSpare(4096, 1500) == 256);          // 476 sopra la scorta -> 256
  CHECK(utilStackSpare(6144, 4000) == 2304);         // 4000-1536=2464 -> 2304
  CHECK(utilStackSpare(3072, 100) == 0);             // stack quasi pieno: niente
  // riga sulla seriale: Backspace cancella davvero (anche sullo schermo), Ctrl+U svuota, le frecce non sporcano la riga
  { String b, e; uint8_t esc = 0;
    CHECK(utilEditKey(b, esc, 'a', e, 10) && b == "a" && e == "a");
    CHECK(utilEditKey(b, esc, 'b', e, 10) && b == "ab");
    CHECK(utilEditKey(b, esc, 127, e, 10) && b == "a" && e == "\b \b");          // Backspace (127 di PuTTY)
    CHECK(utilEditKey(b, esc, 8, e, 10) && b == "" && e == "\b \b");             // Backspace (8)
    CHECK(utilEditKey(b, esc, 127, e, 10) && b == "" && e == "");                // riga vuota: niente da cancellare
    for (char c : String("hello")) utilEditKey(b, esc, c, e, 10);
    CHECK(utilEditKey(b, esc, 21, e, 10) && b == "" && e == "\b \b\b \b\b \b\b \b\b \b");   // Ctrl+U: 5 cancellazioni
    for (char c : String("abc")) utilEditKey(b, esc, c, e, 10);
    CHECK(utilEditKey(b, esc, 27, e, 10) && utilEditKey(b, esc, '[', e, 10) && utilEditKey(b, esc, 'D', e, 10) && b == "abc" && esc == 0 && e == "");   // freccia sinistra: scartata
    CHECK(utilEditKey(b, esc, 27, e, 10) && utilEditKey(b, esc, '[', e, 10) && utilEditKey(b, esc, '3', e, 10) && esc == 2 && utilEditKey(b, esc, '~', e, 10) && esc == 0 && b == "abc");   // Canc
    CHECK(utilEditKey(b, esc, 27, e, 10) && utilEditKey(b, esc, 'O', e, 10) && utilEditKey(b, esc, 'H', e, 10) && b == "abc" && esc == 0);   // Home (ESC O H)
    String full = "1234567890";
    CHECK(utilEditKey(full, esc, 'x', e, 10) && full == "1234567890" && e == "");  // riga piena: non si aggiunge
    CHECK(!utilEditKey(b, esc, '\t', e, 10) && b == "abc");                      // altri caratteri: li gestisce chi chiama
  }
  return done("util");
}
