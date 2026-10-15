#pragma once
// File system FINTO in memoria per la prova prova_iocfg.cpp (prende il posto di tools/stub/FS.h).
// Copia il comportamento di littlefs che conta: il file nuovo prende il posto del vecchio solo a close();
// disco pieno = la write non entra, ma close() conferma lo stesso il file troncato (0 byte);
// taglio della corrente prima di close() = resta il file vecchio (littlefs e a copia-su-scrittura).
#include <Arduino.h>
#include <map>
#include <string>
#include <memory>
#include <time.h>
struct FakeFsFaults { bool full = false; bool cut = false; int opensW = 0; int closes = 0; size_t used = 0; std::string failRenameTo; bool noOpenW = false; };
extern FakeFsFaults g_ff;
namespace fs {
struct FakeNode { std::string data; };
extern std::map<std::string, std::shared_ptr<FakeNode>> g_fakeFiles;
class File: public Stream {
  std::shared_ptr<FakeNode> n; size_t pos = 0; bool wr = false; bool isOpen = false; std::string buf, fpath;
 public:
  File() {}
  File(std::shared_ptr<FakeNode> node, bool w, const std::string& p = "") : n(node), wr(w), isOpen(true), fpath(p) {}
  int peek() { return (isOpen && !wr && pos < n->data.size()) ? (unsigned char)n->data[pos] : -1; }
  int read() { return (isOpen && !wr && pos < n->data.size()) ? (unsigned char)n->data[pos++] : -1; }
  size_t read(uint8_t* b, size_t k) { size_t i = 0; while (i < k) { int c = read(); if (c < 0) break; b[i++] = (uint8_t)c; } return i; }
  int available() { return (isOpen && !wr && pos < n->data.size()) ? (int)(n->data.size() - pos) : 0; }
  size_t write(uint8_t c) { return write(&c, 1); }
  size_t write(const uint8_t* b, size_t k) {
    if (!isOpen || !wr || g_ff.full || g_ff.cut) return 0;       // disco pieno: la scrittura non entra; corrente tagliata: nulla arriva al file
    buf.append((const char*)b, k); return k;
  }
  size_t size() const { return (isOpen && n) ? n->data.size() : 0; }
  void close() { if (isOpen && wr && !g_ff.cut) { auto nn = std::make_shared<FakeNode>(); nn->data = buf; g_fakeFiles[fpath] = nn; } if (isOpen) g_ff.closes++; isOpen = false; }
  operator bool() const { return isOpen; }
  bool isDirectory() { return false; }
  File openNextFile(const char* = "r") { return File(); }
  const char* name() const { return ""; }
  const char* path() const { return ""; }
  bool seek(uint32_t p) { pos = p; return true; }
  size_t position() const { return pos; }
  time_t getLastWrite() { return 0; }
  void flush() {}
};
class FS {
 public:
  File open(const String& p, const char* m = "r", bool = false) {
    std::string k = p.c_str();
    bool w = m && (m[0] == 'w' || m[0] == 'a');
    auto it = g_fakeFiles.find(k);
    if (!w) { if (it == g_fakeFiles.end()) return File(); return File(it->second, false); }
    g_ff.opensW++;
    if (g_ff.noOpenW) return File();
    return File(nullptr, true, k);                                  // "w": il vecchio resta finche non arriva close()
  }
  File open(const char* p, const char* m = "r", bool b = false) { return open(String(p), m, b); }
  bool exists(const String& p) { return g_fakeFiles.count(p.c_str()) > 0; }
  bool exists(const char* p) { return exists(String(p)); }
  bool remove(const String& p) { return g_fakeFiles.erase(p.c_str()) > 0; }
  bool remove(const char* p) { return remove(String(p)); }
  bool rename(const String& a, const String& b) { if (!g_ff.failRenameTo.empty() && g_ff.failRenameTo == b.c_str() && a.endsWith("tmp~")) return false; auto it = g_fakeFiles.find(a.c_str()); if (it == g_fakeFiles.end()) return false; g_fakeFiles[b.c_str()] = it->second; g_fakeFiles.erase(a.c_str()); return true; }
  bool mkdir(const String&) { return true; }
  bool rmdir(const String&) { return true; }
  size_t totalBytes() { return 640 * 1024; }
  size_t usedBytes() { return g_ff.used; }
  bool begin(bool = false, const char* = "/littlefs", uint8_t = 10, const char* = "spiffs") { return true; }
  bool format() { g_fakeFiles.clear(); return true; }
  void end() {}
};
}
using fs::File; using fs::FS;
