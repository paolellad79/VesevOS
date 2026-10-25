// Prova della lettura a pezzi del modulo multipart del recovery 0.5.0 (recovery/VesevOS_Recovery_IDF/rec_multipart.h).
#include "mini.h"
#include <string>
#include <vector>
#include "../../recovery/VesevOS_Recovery_IDF/rec_multipart.h"

struct Collect : RecMultipart::Sink {
  std::string data; int starts = 0, ends = 0;
  void fileStart() override { starts++; }
  void fileData(const uint8_t* d, size_t n) override { data.append((const char*)d, n); }
  void fileEnd() override { ends++; }
};

static std::string body(const std::string& b, const std::string& file, bool extraField) {
  std::string s = "--" + b + "\r\n";
  if (extraField) s += "Content-Disposition: form-data; name=\"x\"\r\n\r\nvalore\r\n--" + b + "\r\n";
  s += "Content-Disposition: form-data; name=\"fw\"; filename=\"a.bin\"\r\nContent-Type: application/octet-stream\r\n\r\n";
  s += file + "\r\n--" + b + "--\r\n";
  return s;
}

static bool run(const std::string& b, const std::string& m, size_t chunk, Collect& c, bool& finished) {
  RecMultipart mp;
  if (!mp.begin(b, &c)) return false;
  for (size_t i = 0; i < m.size(); i += chunk) if (!mp.feed((const uint8_t*)m.data() + i, i + chunk <= m.size() ? chunk : m.size() - i)) return false;
  finished = mp.finished();
  return true;
}

int main() {
  std::string file;
  for (int i = 0; i < 20000; i++) file += (char)(i * 7 + (i >> 8));
  file += "\r\n--BND\r\nfinto";                      // dentro al file c'e qualcosa che somiglia al confine
  std::string b = "----WebKitFormBoundaryAbC123";
  size_t chunks[] = {1, 2, 3, 7, 50, 100, 4096, 100000};
  for (bool extra : {false, true}) for (size_t ch : chunks) {
    Collect c; bool fin = false;
    std::string m = body(b, file, extra);
    bool ok = run(b, m, ch, c, fin);
    CHECK(ok); CHECK(fin); CHECK(c.starts == 1); CHECK(c.ends == 1); CHECK(c.data == file);
  }
  { Collect c; bool fin = false; CHECK(run(b, body(b, "", false), 5, c, fin)); CHECK(c.starts == 1 && c.ends == 1 && c.data.empty()); }
  { Collect c; bool fin = false; std::string m = body(b, file, false); m.resize(m.size() - 30);   // tagliato: il file non e finito
    CHECK(run(b, m, 64, c, fin)); CHECK(!fin); CHECK(c.starts == 1 && c.ends == 0); }
  { Collect c; RecMultipart mp; CHECK(!mp.begin("", &c)); }
  { Collect c; bool fin = false; CHECK(run(b, "--" + b + "\r\nContent-Disposition: form-data; name=\"x\"\r\n\r\nciao\r\n--" + b + "--\r\n", 9, c, fin)); CHECK(c.starts == 0); }
  { Collect c; RecMultipart mp; mp.begin(b, &c); std::string m = "--" + b + "\r\n" + std::string(3000, 'h'); CHECK(!mp.feed((const uint8_t*)m.data(), m.size())); }
  return done("test_recmulti");
}
