// Prova delle regole del recovery per tema e messaggio (rec_vista_regole.h). Compilare con: g++ -std=gnu++17 -I. test_recvista.cpp -o test_recvista
#include "mini.h"
#include "../../recovery/VesevOS_Recovery_IDF/rec_vista_regole.h"

int main() {
  CHECK(recTemaControlla("") == RT_VUOTO);
  CHECK(recTemaControlla(":root{--ac:#c00}") == RT_OK);
  CHECK(recTemaControlla(std::string(REC_TEMA_MAX, 'a')) == RT_OK);
  CHECK(recTemaControlla(std::string(REC_TEMA_MAX + 1, 'a')) == RT_GRANDE);
  CHECK(recTemaControlla(std::string("a\0b", 3)) == RT_NUL);
  CHECK(recTemaControlla("a{b:url(x)}") == RT_VIETATO);
  CHECK(recTemaControlla("@IMPORT 'x';") == RT_VIETATO);
  CHECK(recTemaControlla("a{background:URL(x)}") == RT_VIETATO);
  CHECK(recTemaControlla("a{b:\\75rl(x)}") == RT_VIETATO);
  CHECK(recTemaControlla("</style><script>") == RT_VIETATO);
  CHECK(recTemaControlla("a{b:image-set(x)}") == RT_VIETATO);
  CHECK(recTemaControlla("a{b:expression(1)}") == RT_VIETATO);
  CHECK(recTemaControlla("a{b:javascript:x}") == RT_VIETATO);
  CHECK(recTemaControlla("a{-moz-binding:x}") == RT_VIETATO);

  std::string o;
  CHECK(recMsgPulisci("Orari 9-18", o) && o == "Orari 9-18");
  CHECK(recMsgPulisci("  ciao\r\nmondo \n", o) && o == "ciao\nmondo");
  CHECK(!recMsgPulisci("perch\xc3\xa9", o) && o.empty());
  CHECK(!recMsgPulisci("a\\b", o));
  CHECK(recMsgPulisci(std::string(REC_MSG_MAX, 'x'), o));
  CHECK(!recMsgPulisci(std::string(REC_MSG_MAX + 1, 'x'), o));
  CHECK(recMsgDecodifica("uno\\ndue") == "uno\ndue");
  CHECK(recMsgDecodifica("fine\\") == "fine\\");
  CHECK(recJsonTesto("a\"b\\c\nd<e>&'") == "a\\u0022b\\u005cc\\nd\\u003ce\\u003e\\u0026\\u0027");
  CHECK(recJsonTesto("</script>").find('<') == std::string::npos);
  return done("recvista");
}
