// VesevOS Recovery - regole pure per il tema e per il messaggio della pagina (nessuna dipendenza, provate sul computer da tools/hosttest/test_recvista.cpp).
// Sono la copia delle regole di VesevOS: tema = core/vos_tema_rule.h (file /etc/tema.css), messaggio = core/vos_msglogin.cpp (opzione messaggiologin).
// Se cambiano in VesevOS, vanno cambiate anche qui.
#pragma once
#include <stddef.h>
#include <string>

#define REC_TEMA_MAX 32768       // byte al massimo di un tema (come VesevOS)
#define REC_MSG_MAX 200          // caratteri al massimo del messaggio (come VesevOS)

enum RecTemaEsito { RT_OK = 0, RT_VUOTO, RT_GRANDE, RT_NUL, RT_VIETATO };

// Un tema e rifiutato per intero se e vuoto, piu grande di REC_TEMA_MAX, ha byte nulli, una barra rovesciata, '<' o una di queste parole
// (senza badare alle maiuscole). Cosi non puo caricare nulla da un altro indirizzo.
inline RecTemaEsito recTemaControlla(const std::string& t) {
  if (t.empty()) return RT_VUOTO;
  if (t.size() > REC_TEMA_MAX) return RT_GRANDE;
  std::string low;
  low.reserve(t.size());
  for (char c : t) {
    if (c == 0) return RT_NUL;
    if (c == '\\' || c == '<') return RT_VIETATO;
    if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
    low += c;
  }
  static const char* const kVietate[] = {"@import", "url(", "image-set(", "src(", "expression(", "javascript:", "-moz-binding"};
  for (const char* w : kVietate)
    if (low.find(w) != std::string::npos) return RT_VIETATO;
  return RT_OK;
}

inline const char* recTemaEsitoNome(RecTemaEsito e) {
  switch (e) {
    case RT_OK: return "ok";
    case RT_VUOTO: return "vuoto";
    case RT_GRANDE: return "troppo grande";
    case RT_NUL: return "byte nullo";
    default: return "parole vietate";
  }
}

// Nel file l'a capo del messaggio e scritto come i due caratteri "\n".
inline std::string recMsgDecodifica(const std::string& in) {
  std::string out;
  for (size_t i = 0; i < in.size(); i++) {
    if (in[i] == '\\' && i + 1 < in.size() && in[i + 1] == 'n') {
      out += '\n';
      i++;
    } else {
      out += in[i];
    }
  }
  return out;
}

// Pulisce e controlla il messaggio (testo gia decodificato): solo ASCII 32-126 e a capo, niente barra rovesciata, al massimo REC_MSG_MAX
// caratteri. Toglie i \r e gli spazi o gli a capo ai lati. false = non valido: out resta vuoto.
inline bool recMsgPulisci(const std::string& in, std::string& out) {
  out.clear();
  std::string t;
  for (char c : in) {
    if (c == '\r') continue;
    bool ok = c == '\n' || (c >= 32 && c <= 126 && c != '\\');
    if (!ok || t.size() >= REC_MSG_MAX) return false;
    t += c;
  }
  size_t a = 0;
  size_t b = t.size();
  while (a < b && (t[a] == ' ' || t[a] == '\n')) a++;
  while (b > a && (t[b - 1] == ' ' || t[b - 1] == '\n')) b--;
  out = t.substr(a, b - a);
  return true;
}

// Testo per dentro una stringa JSON: tutto cio che potrebbe uscire dalla stringa (apici, barre, segni di tag, caratteri di controllo) e scritto con la sequenza di escape.
inline std::string recJsonTesto(const std::string& in) {
  static const char hex[] = "0123456789abcdef";
  std::string out;
  for (unsigned char c : in) {
    if (c == '\n') {
      out += "\\n";
    } else if (c < 32 || c > 126 || c == '"' || c == '\\' || c == '<' || c == '>' || c == '&' || c == '\'') {
      out += "\\u00";
      out += hex[c >> 4];
      out += hex[c & 15];
    } else {
      out += (char)c;
    }
  }
  return out;
}
