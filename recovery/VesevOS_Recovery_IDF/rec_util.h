// VesevOS Recovery - piccoli aiuti: tempo, esadecimale, SHA-256, HMAC, numeri casuali, testo di un modulo web.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>

uint32_t recMillis();                                              // millisecondi dall'avvio (si ripete dopo 49 giorni, come millis())
std::string hexOf(const uint8_t* b, size_t n);
std::string sha256Hex(const std::string& s);
std::string hmacHex(const std::string& key, const std::string& msg);   // "" se qualcosa non va
std::string randomHex(int bytes);                                  // al massimo 32 byte
bool sameStr(const std::string& a, const std::string& b);          // confronto a tempo costante

std::string urlDecode(const std::string& s);                       // %xx e + (spazio)
std::string formGet(const std::string& body, const char* name);    // valore di un campo "a=1&b=2"; "" se manca
std::string trimStr(const std::string& s);
