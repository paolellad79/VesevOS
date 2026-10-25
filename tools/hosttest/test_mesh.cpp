// VesevOS - prove sul computer: lettura dei messaggi della rete tra schede e JSON di stato (packages/vos_mesh_msg.h)
#include "vos_mesh_msg.h"
#include "mini.h"
#include <stdio.h>
#include <string>

static std::string run(const MeshJsonIn& in) { return meshBuild([&](StrBuf& sb) { meshJsonBuild(sb, in); }); }

int main() {
  // ---- meshCopy: tronca e chiude sempre con zero ----
  { char b[5]; meshCopy(b, sizeof b, "abcdefgh"); CHECK(!strcmp(b, "abcd")); meshCopy(b, sizeof b, "xy"); CHECK(!strcmp(b, "xy"));
    meshCopy(b, sizeof b, std::string_view("")); CHECK(b[0] == 0); char c[1] = {'z'}; meshCopy(c, 1, "abc"); CHECK(c[0] == 0); }

  // ---- azioni ammesse: conta la prima parola ----
  CHECK(meshCmdAllowed("led 1")); CHECK(meshCmdAllowed("led")); CHECK(meshCmdAllowed("led-color 255 0 0")); CHECK(meshCmdAllowed("led-bright 10"));
  CHECK(meshCmdAllowed("gpio 5 1")); CHECK(meshCmdAllowed("note ciao a tutti")); CHECK(meshCmdAllowed("reboot")); CHECK(meshCmdAllowed("ntp sync"));
  CHECK(!meshCmdAllowed("")); CHECK(!meshCmdAllowed(" led")); CHECK(!meshCmdAllowed("ledx")); CHECK(!meshCmdAllowed("wait 5")); CHECK(!meshCmdAllowed("LED"));
  CHECK(!meshCmdAllowed("shell rm /")); CHECK(!meshCmdAllowed("reboot2 now"));

  // ---- MAC ----
  { uint8_t m[6] = {0x0A, 0xBC, 0x01, 0xFF, 0x00, 0x7E}; char s[18]; meshMacStr(m, s); CHECK(!strcmp(s, "0A:BC:01:FF:00:7E")); }

  // ---- chiave comune ----
  { uint8_t k[32]; std::string ok(64, 'a'); ok[0] = '0'; ok[1] = '1'; CHECK(meshKeyParse(ok, k)); CHECK(k[0] == 0x01 && k[1] == 0xAA);
    CHECK(!meshKeyParse(std::string(63, 'a'), k)); CHECK(!meshKeyParse(std::string(65, 'a'), k)); CHECK(!meshKeyParse("", k));
    std::string bad(64, 'a'); bad[0] = 'z'; bad[1] = 'z'; CHECK(meshKeyParse(bad, k)); CHECK(k[0] == 0); }   // come prima: coppia non valida = 0

  // ---- nomi nei messaggi ----
  CHECK(meshBeaconName("11|casa") == "casa"); CHECK(meshBeaconName("solonome") == "solonome"); CHECK(meshBeaconName("|x") == "x"); CHECK(meshBeaconName("") == "");
  CHECK(meshStateName("nodo1|1.2.3.4|-50") == "nodo1"); CHECK(meshStateName("senzabarra") == "senzabarra"); CHECK(meshStateName("|abc") == "|abc"); CHECK(meshStateName("") == "");

  // ---- divisione dello stato ----
  { std::string_view f[MESH_FIELDS];
    meshSplitState("a|b|c|d|e|f|g", f); CHECK(f[0] == "a" && f[1] == "b" && f[6] == "g");
    meshSplitState("a|b|c", f); CHECK(f[0] == "a" && f[2] == "c" && f[3].empty() && f[6].empty());
    meshSplitState("a|b|c|d|e|f|g|h|i", f); CHECK(f[6] == "g");                  // oltre il settimo campo si ignora
    meshSplitState("", f); CHECK(f[0].empty() && f[1].empty());
    meshSplitState("||", f); CHECK(f[0].empty() && f[1].empty() && f[2].empty() && f[3].empty());
    meshSplitState("solo", f); CHECK(f[0] == "solo" && f[1].empty()); }

  // ---- testo di un messaggio ----
  { std::string t; size_t n = meshCleanText("ciao\r\nmondo\tx", t, 151); CHECK(t == "ciao mondo\tx" && n == t.size());
    t.clear(); n = meshCleanText("a\x01" "b\xC3\xA9", t, 151); CHECK(t == "a?b??" && n == 5);   // fuori da ASCII: un '?' per byte
    t.clear(); n = meshCleanText("", t, 151); CHECK(n == 0 && t.empty());
    t.clear(); n = meshCleanText(nullptr, t, 151); CHECK(n == 0);
    std::string lungo(400, 'x'); t.clear(); n = meshCleanText(lungo.c_str(), t, 151); CHECK(n == 400 && t.size() == 151);   // la lunghezza vera si conosce, si tiene solo l'inizio
    t.clear(); n = meshCleanText("\r\r\r", t, 151); CHECK(n == 0); }

  // ---- escape JSON e numeri ----
  { char b[64]; StrBuf sb(b, sizeof b); meshJsonStr(sb, "a\"b\\c\n\r\t\x01z\x80"); CHECK(!strcmp(b, "a\\\"b\\\\c\\n\\r\\t?z?"));
    auto num = [](std::string_view v) { char t[32]; StrBuf s(t, sizeof t); meshJsonNum(s, v); return std::string(t); };
    CHECK(num("-50") == "-50"); CHECK(num("0") == "0"); CHECK(num("123456") == "123456"); CHECK(num("") == "0"); CHECK(num("-") == "0");
    CHECK(num("12,\"admin\":true") == "0"); CHECK(num("1e5") == "0"); CHECK(num("+5") == "0"); CHECK(num("123456789012") == "0"); CHECK(num("12345678901") == "12345678901");
    CHECK(num("5 ") == "0"); CHECK(num("--5") == "0"); }

  // ---- JSON dello stato di un nodo per MQTT ----
  { std::string_view f[MESH_FIELDS]; meshSplitState("nodo1|192.168.1.7|-61|3600|41|12|1.7.66a", f);
    char b[256]; StrBuf sb(b, sizeof b); meshStateJson(sb, f, 2); CHECK(!sb.truncated());
    CHECK(!strcmp(b, "{\"ip\":\"192.168.1.7\",\"rssi\":-61,\"uptime\":3600,\"temp\":41,\"cpu\":12,\"ver\":\"1.7.66a\",\"hops\":2}"));
    meshSplitState("n|ip\"x|1,\"a\":1|2|3|4|v\"", f);                          // un nodo che prova a infilare altro JSON
    StrBuf s2(b, sizeof b); meshStateJson(s2, f, 0);
    CHECK(!strcmp(b, "{\"ip\":\"ip\\\"x\",\"rssi\":0,\"uptime\":2,\"temp\":3,\"cpu\":4,\"ver\":\"v\\\"\",\"hops\":0}"));
    meshSplitState("n", f); StrBuf s3(b, sizeof b); meshStateJson(s3, f, 1);   // campi mancanti: 0 e testo vuoto
    CHECK(!strcmp(b, "{\"ip\":\"\",\"rssi\":0,\"uptime\":0,\"temp\":0,\"cpu\":0,\"ver\":\"\",\"hops\":1}"));
    char piccolo[20]; StrBuf s4(piccolo, sizeof piccolo); meshSplitState("a|b|c|d|e|f|g", f); meshStateJson(s4, f, 1); CHECK(s4.truncated()); }   // non entra: chi usa controlla truncated()

  // ---- JSON di /api/mesh ----
  { MeshNode nodes[3] = {}; MeshMsg msgs[3] = {};
    nodes[0].used = true; nodes[0].mac[0] = 0xAA; nodes[0].mac[5] = 0x01; strcpy(nodes[0].name, "casa"); nodes[0].role = 1; nodes[0].hops = 0; nodes[0].rssi = -40; nodes[0].last = 4000; strcpy(nodes[0].state, "casa|1.2.3.4|-40|5|30|1|v");
    nodes[2].used = true; nodes[2].mac[5] = 0x02; strcpy(nodes[2].name, "s\"x"); nodes[2].hops = 2; nodes[2].ccx = true; nodes[2].last = 1000;
    strcpy(msgs[0].from, "a"); strcpy(msgs[0].text, "primo"); strcpy(msgs[0].when, "t0");
    strcpy(msgs[1].from, "b"); strcpy(msgs[1].text, "se\"condo"); strcpy(msgs[1].when, "t1");
    MeshJsonIn in = { true, false, true, 1, 11, 13, "AA:BB", 7, 8, 9, 10000, nodes, 3, msgs, 2 };
    std::string j = run(in);
    const char* atteso = "{\"run\":true,\"auto\":false,\"role\":1,\"hasKey\":true,\"ch\":11,\"chMax\":13,\"me\":\"AA:BB\",\"sent\":7,\"recv\":8,\"bad\":9,\"nodes\":["
      "{\"mac\":\"AA:00:00:00:00:01\",\"name\":\"casa\",\"role\":1,\"hops\":0,\"rssi\":-40,\"ago\":6,\"ccx\":false,\"state\":\"casa|1.2.3.4|-40|5|30|1|v\"},"
      "{\"mac\":\"00:00:00:00:00:02\",\"name\":\"s\\\"x\",\"role\":0,\"hops\":2,\"rssi\":0,\"ago\":9,\"ccx\":true,\"state\":\"\"}],"
      "\"msgs\":[{\"from\":\"b\",\"text\":\"se\\\"condo\",\"when\":\"t1\"},{\"from\":\"a\",\"text\":\"primo\",\"when\":\"t0\"}]}";
    CHECK(j == atteso);
    MeshJsonIn vuoto = { false, false, false, 0, 1, 13, "", 0, 0, 0, 0, nullptr, 0, nullptr, 0 };
    CHECK(run(vuoto) == "{\"run\":false,\"auto\":false,\"role\":0,\"hasKey\":false,\"ch\":1,\"chMax\":13,\"me\":\"\",\"sent\":0,\"recv\":0,\"bad\":0,\"nodes\":[],\"msgs\":[]}"); }

  // ---- caso peggiore: 16 nodi e 10 messaggi pieni di caratteri da proteggere: il JSON entra intero e resta valido ----
  { static MeshNode nodes[16]; static MeshMsg msgs[10];
    for (int i = 0; i < 16; i++) { nodes[i].used = true; memset(nodes[i].name, '"', MESH_NAME_CAP - 1); memset(nodes[i].state, '\\', MESH_STATE_CAP - 1); nodes[i].mac[0] = (uint8_t)i; nodes[i].hops = 3; nodes[i].rssi = -128; nodes[i].last = 1; }
    for (int i = 0; i < 10; i++) { memset(msgs[i].from, '"', MESH_FROM_CAP - 1); memset(msgs[i].text, '\n', MESH_MAXPAY); memset(msgs[i].when, '"', MESH_WHEN_CAP - 1); }
    MeshJsonIn in = { true, true, true, 2, 14, 14, "FF:FF:FF:FF:FF:FF", 4294967295u, 4294967295u, 4294967295u, 4294967295u, nodes, 16, msgs, 10 };
    std::string j = run(in); CHECK(!j.empty() && j.front() == '{' && j.back() == '}'); CHECK(j.size() > 8000 && j.size() < 20000);
    // la misura esatta coincide con quella scritta
    StrBuf conta(nullptr, 0); meshJsonBuild(conta, in); CHECK(conta.required() == j.size()); }

  // ---- meshBuild: se i dati cambiano e non entra, ritorna vuoto (mai testo a meta) ----
  { int chiamate = 0; std::string r = meshBuild([&](StrBuf& sb) { chiamate++; for (int i = 0; i < chiamate * 3000; i++) sb.add('x'); }); CHECK(r.empty()); }
  { std::string r = meshBuild([&](StrBuf& sb) { sb.add("ciao"); }); CHECK(r == "ciao"); }
  { int chiamate = 0; std::string r = meshBuild([&](StrBuf& sb) { chiamate++; sb.add("ab"); if (chiamate == 2) sb.add("cdef"); }); CHECK(r == "abcdef"); }   // cresce di poco: il margine basta

  // ---- la vista non ha lo zero finale: prove con ASan su un blocco della misura esatta (niente byte dopo la fine) ----
  { const char* lit = "nodo1|10.0.0.9|-70|12|40|3|1.7.66a"; size_t n = strlen(lit); char* blk = (char*)malloc(n); memcpy(blk, lit, n);
    std::string_view pay(blk, n);
    CHECK(meshStateName(pay) == "nodo1"); std::string_view f[MESH_FIELDS]; meshSplitState(pay, f); CHECK(f[6] == "1.7.66a" && f[2] == "-70");
    char b[200]; StrBuf sb(b, sizeof b); meshStateJson(sb, f, 1); CHECK(!sb.truncated());
    std::string_view v2(blk, 5); CHECK(meshBeaconName(v2) == "nodo1");
    free(blk); }

  return done("test_mesh");
}
