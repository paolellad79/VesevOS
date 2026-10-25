#pragma once
// File system FINTO in memoria CON CARTELLE per la prova prova_files.cpp (prende il posto di tools/stub/FS.h).
// Copia le regole di littlefs che contano per vos_files: rename sopra un file esistente lo sostituisce, una cartella si cancella solo
// se e vuota, ogni apertura va chiusa. In piu controlla due cose che sulla scheda sarebbero guai: una voce cancellata o spostata
// MENTRE una cartella e aperta per scorrerla (g_df.mutatedWhileOpen) e le aperture rimaste senza close (g_df.open).
#include <Arduino.h>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <time.h>
struct DirFsFaults {
  bool full = false;            // disco pieno: la write non entra
  bool fullNames = false;       // name() da' il percorso intero (alcune versioni del core) invece del nome corto
  size_t total = 64 * 1024;     // totalBytes()
  int open = 0;                 // aperture senza close
  int openDirs = 0;             // cartelle aperte adesso
  bool mutatedWhileOpen = false;
  int removes = 0, renames = 0, writesOpened = 0;
  std::string failRemove;       // percorso la cui remove() fallisce
  std::string failRename;       // destinazione la cui rename() fallisce
};
extern DirFsFaults g_df;
namespace fs {
struct DNode { bool dir = false; std::string data; };
extern std::map<std::string, DNode> g_tree;     // chiave = percorso intero ("/a/b"); la radice "/" non e nella mappa ma esiste sempre
std::string dfParent(const std::string& p);
bool dfIsDir(const std::string& p);
std::vector<std::string> dfKids(const std::string& dir);   // nomi corti dei figli diretti, in ordine
class File: public Stream {
  struct Impl {
    std::string path; bool dir = false, wr = false; size_t pos = 0; std::vector<std::string> kids; size_t kid = 0; std::string nm;
    ~Impl() { g_df.open--; if (dir) g_df.openDirs--; }
  };
  std::shared_ptr<Impl> i_;
 public:
  File() {}
  File(const std::string& p, bool dir, bool w) : i_(std::make_shared<Impl>()) {
    i_->path = p; i_->dir = dir; i_->wr = w; g_df.open++;
    if (dir) { g_df.openDirs++; i_->kids = dfKids(p); }
  }
  int peek() { return (i_ && !i_->wr && !i_->dir && i_->pos < data().size()) ? (unsigned char)data()[i_->pos] : -1; }
  int read() { return (i_ && !i_->wr && !i_->dir && i_->pos < data().size()) ? (unsigned char)data()[i_->pos++] : -1; }
  size_t read(uint8_t* b, size_t k) { size_t n = 0; while (n < k) { int c = read(); if (c < 0) break; b[n++] = (uint8_t)c; } return n; }
  int available() { return (i_ && !i_->wr && !i_->dir && i_->pos < data().size()) ? (int)(data().size() - i_->pos) : 0; }
  size_t write(uint8_t c) { return write(&c, 1); }
  size_t write(const uint8_t* b, size_t k) {
    if (!i_ || !i_->wr || g_df.full) return 0;
    auto it = g_tree.find(i_->path); if (it == g_tree.end()) return 0;
    it->second.data.append((const char*)b, k); return k;
  }
  size_t size() const { return i_ && !i_->dir ? dataC().size() : 0; }
  void close() { i_.reset(); }
  operator bool() const { return (bool)i_; }
  bool isDirectory() { return i_ && i_->dir; }
  File openNextFile(const char* = "r") {
    if (!i_ || !i_->dir || i_->kid >= i_->kids.size()) return File();
    std::string full = (i_->path == "/" ? "" : i_->path) + "/" + i_->kids[i_->kid++];
    auto it = g_tree.find(full); if (it == g_tree.end()) return File();
    return File(full, it->second.dir, false);
  }
  const char* name() const { if (!i_) return ""; if (g_df.fullNames) return i_->path.c_str(); size_t s = i_->path.rfind('/'); i_->nm = i_->path.substr(s + 1); return i_->nm.c_str(); }
  const char* path() const { return i_ ? i_->path.c_str() : ""; }
  bool seek(uint32_t p) { if (i_) i_->pos = p; return true; }
  size_t position() const { return i_ ? i_->pos : 0; }
  time_t getLastWrite() { return 0; }
  void flush() {}
 private:
  const std::string& dataC() const { static std::string e; auto it = g_tree.find(i_->path); return it == g_tree.end() ? e : it->second.data; }
  const std::string& data() { return dataC(); }
};
class FS {
  static void mutated() { if (g_df.openDirs > 0) g_df.mutatedWhileOpen = true; }
 public:
  File open(const std::string& p, const char* m) {
    bool w = m && (m[0] == 'w' || m[0] == 'a');
    if (p == "/") return w ? File() : File("/", true, false);
    auto it = g_tree.find(p);
    if (!w) return it == g_tree.end() ? File() : File(p, it->second.dir, false);
    if (it != g_tree.end() && it->second.dir) return File();
    if (!dfIsDir(dfParent(p))) return File();
    g_df.writesOpened++;
    if (it == g_tree.end()) { mutated(); g_tree[p] = DNode(); } else if (m[0] == 'w') it->second.data.clear();
    return File(p, false, true);
  }
  File open(const String& p, const char* m = "r", bool = false) { return open(std::string(p.c_str()), m); }
  File open(const char* p, const char* m = "r", bool = false) { return open(std::string(p), m); }
  bool exists(const char* p) { return std::string(p) == "/" || g_tree.count(p) > 0; }
  bool exists(const String& p) { return exists(p.c_str()); }
  bool remove(const char* p) {
    std::string k = p; auto it = g_tree.find(k);
    if (it == g_tree.end() || it->second.dir || k == g_df.failRemove) return false;
    mutated(); g_tree.erase(it); g_df.removes++; return true;
  }
  bool remove(const String& p) { return remove(p.c_str()); }
  bool rename(const char* a, const char* b) {
    std::string ka = a, kb = b; auto ia = g_tree.find(ka);
    if (ia == g_tree.end() || kb == g_df.failRename || !dfIsDir(dfParent(kb))) return false;
    auto ib = g_tree.find(kb);
    if (ib != g_tree.end()) {                                  // come lfs_rename: stesso tipo, e se e una cartella deve essere vuota
      if (ib->second.dir != ia->second.dir) return false;
      if (ib->second.dir && !dfKids(kb).empty()) return false;
    }
    mutated(); g_df.renames++;
    std::map<std::string, DNode> moved;
    for (auto it = g_tree.begin(); it != g_tree.end();) {
      if (it->first == ka || it->first.compare(0, ka.size() + 1, ka + "/") == 0) { moved[kb + it->first.substr(ka.size())] = it->second; it = g_tree.erase(it); }
      else ++it;
    }
    for (auto& kv : moved) g_tree[kv.first] = kv.second;
    return true;
  }
  bool rename(const String& a, const String& b) { return rename(a.c_str(), b.c_str()); }
  bool mkdir(const char* p) {
    std::string k = p;
    if (k == "/" || g_tree.count(k) || !dfIsDir(dfParent(k))) return false;
    mutated(); g_tree[k].dir = true; return true;
  }
  bool mkdir(const String& p) { return mkdir(p.c_str()); }
  bool rmdir(const char* p) {
    std::string k = p; auto it = g_tree.find(k);
    if (it == g_tree.end() || !it->second.dir || !dfKids(k).empty()) return false;
    mutated(); g_tree.erase(it); return true;
  }
  bool rmdir(const String& p) { return rmdir(p.c_str()); }
  size_t totalBytes() { return g_df.total; }
  size_t usedBytes() { size_t n = 0; for (auto& kv : g_tree) n += kv.second.data.size(); return n; }
  bool begin(bool = false, const char* = "/littlefs", uint8_t = 10, const char* = "spiffs") { return true; }
  bool format() { g_tree.clear(); return true; }
  void end() {}
};
}
using fs::File; using fs::FS;
