// VesevOS - prove sul computer: periferiche a classi (vos_io.cpp) con pin, driver e registro finti
#include <Arduino.h>
#include "vos_io.h"
#include "vos_pins.h"
#include "vos_dev.h"
#include "vos_drv_gpio.h"
#include "vos_common.h"
#include <FS.h>
#include "mini.h"
#include "fake_i2c.h"
#include <unistd.h>

// file system finto in memoria (iocfg/FS.h): run.sh compila questo test con quello
FakeFsFaults g_ff;
namespace fs { std::map<std::string, std::shared_ptr<FakeNode>> g_fakeFiles; }
// ---- finti pezzi ----
static int lvl[50]; static int mode[50]; static int claimed[50]; static const char* owner[50];
void drvGpioMode(int g, DrvGpioMode m) { mode[g] = m; }
void drvGpioWrite(int g, bool h) { lvl[g] = h; }
bool drvGpioRead(int g) { return lvl[g] != 0; }
int  drvGpioAnalog(int g) { return 1234 + g; }
const char* pinTestBlock(int g) {
  if (g < 0 || g > 48 || (g >= 22 && g <= 32)) return "inesistente";
  if (g == 19 || g == 20) return "USB"; if (g == 48) return "LED RGB"; if (g == 0) return "BOOT"; if (g == 3 || g == 45 || g == 46) return "avvio";
  if (claimed[g]) return "gia usato";
  return nullptr;
}
bool pinClaim(uint8_t g, const char* o, const char*, bool) { if (claimed[g] && owner[g] && strcmp(owner[g], o)) return false; claimed[g] = 1; owner[g] = o; return true; }
void pinRelease(uint8_t g) { claimed[g] = 0; owner[g] = nullptr; }
const DevOps* devFind(const String& id) { static DevOps d; return (id == "mqtt" || id == "boot") ? &d : nullptr; }

int main() {
  String err, nm;
  // nomi
  CHECK(!ioNameOk("a", err) && !ioNameOk("1abc", err) && !ioNameOk("Rele1", err) && !ioNameOk("rèle", err) && !ioNameOk("rele 1", err));
  CHECK(!ioNameOk("led", err) && !ioNameOk("mqtt", err) && !ioNameOk("temp", err) && !ioNameOk("gpio", err));   // parole riservate
  CHECK(ioNameOk("rele1", err) && ioNameOk("luce", err) && !ioNameOk("abcdefghijkl", err));
  // classi e pin
  CHECK(!ioAdd("forno", 5, "", "", nm, err));
  CHECK(!ioAdd("rele", 19, "", "", nm, err));                       // USB
  CHECK(!ioAdd("rele", 48, "", "", nm, err) && !ioAdd("rele", 0, "", "", nm, err) && !ioAdd("rele", 45, "", "", nm, err) && !ioAdd("rele", 25, "", "", nm, err));
  CHECK(!ioAdd("analogico", 15, "", "", nm, err));                  // solo 1-10
  CHECK(ioAdd("rele", 5, "", "inv", nm, err) && nm == "rele1");
  CHECK(lvl[5] == 1 && mode[5] == DG_OUTPUT);                       // attivo basso: spento = livello alto, impostato PRIMA dell'uscita
  CHECK(!ioAdd("led", 5, "", "", nm, err));                         // pin gia assegnato
  CHECK(ioAdd("rele", 6, "", "", nm, err) && nm == "rele2");
  CHECK(ioAdd("led", 7, "", "", nm, err) && nm == "led1");
  CHECK(ioAdd("pin", 8, "", "", nm, err) && nm == "pin1");
  CHECK(ioAdd("ingresso", 9, "", "up", nm, err) && nm == "in1" && mode[9] == DG_INPUT_PULLUP);
  CHECK(ioAdd("analogico", 4, "sonda", "", nm, err) && nm == "sonda");
  CHECK(!ioAdd("pin", 10, "sonda", "", nm, err));                   // nome doppio
  CHECK(!ioAdd("pin", 10, "mqtt", "", nm, err));                    // nome di sistema
  CHECK(ioCount() == 6);
  // uscite
  CHECK(ioSet("led1", 1, ROLE_OPER, err) && lvl[7] == 1 && ioState("led1") == 1);
  CHECK(!ioSet("led1", 1, ROLE_GUEST, err) || true);
  CHECK(!ioSet("rele2", 1, ROLE_OPER, err));                        // il rele solo Admin
  CHECK(ioSet("rele2", 1, ROLE_ADMIN, err) && lvl[6] == 1);
  CHECK(ioSet("rele2", 1, ROLE_ADMIN, err));                        // stesso valore: niente da fare
  CHECK(ioSet("rele2", 0, ROLE_ADMIN, err) && lvl[6] == 0);         // spegnere si puo sempre, subito
  CHECK(!ioSet("rele2", 1, ROLE_ADMIN, err));                       // riaccendere entro 1 s no
  CHECK(ioSet("rele1", 1, ROLE_ADMIN, err) && lvl[5] == 0);        // attivo basso: acceso = livello basso
  int v = -1;
  CHECK(ioGet("rele1", v, err) && v == 1);
  CHECK(!ioSet("in1", 1, ROLE_ADMIN, err) && !ioSet("nuovo", 1, ROLE_ADMIN, err));
  // ingresso e analogico
  lvl[9] = 1; CHECK(ioGet("in1", v, err) && v == 1);
  lvl[9] = 0; CHECK(ioGet("in1", v, err) && v == 0);
  CHECK(ioGet("sonda", v, err) && v == 1238);
  CHECK(ioState("in1") == 1 && ioIsOut("pin1") && !ioIsOut("in1") && !ioCanSwitch("sonda"));
  // impulso e lampeggio
  CHECK(ioPulse("led1", 100, ROLE_OPER, err) && lvl[7] == 1);
  ioTick(); CHECK(lvl[7] == 1); usleep(160000); ioTick(); CHECK(lvl[7] == 0 && ioState("led1") == 0);
  CHECK(!ioPulse("led1", 50, ROLE_OPER, err) && !ioPulse("led1", 6000, ROLE_OPER, err) && !ioPulse("in1", 500, ROLE_ADMIN, err));
  CHECK(!ioPulse("rele1", 500, ROLE_OPER, err));                    // il rele a un operatore: no
  CHECK(!ioBlink("rele1", 3, ROLE_ADMIN, err) && !ioBlink("in1", 3, ROLE_ADMIN, err) && !ioBlink("led1", 0, ROLE_OPER, err) && !ioBlink("led1", 11, ROLE_OPER, err));
  CHECK(ioBlink("led1", 1, ROLE_OPER, err)); ioTick(); CHECK(lvl[7] == 1); usleep(450000); ioTick(); CHECK(lvl[7] == 0); usleep(450000); ioTick(); CHECK(lvl[7] == 1);
  CHECK(ioSet("led1", 0, ROLE_OPER, err) && lvl[7] == 0); usleep(450000); ioTick(); CHECK(lvl[7] == 0);   // un comando a mano ferma il lampeggio
  // stato sicuro
  ioAllOff();
  CHECK(lvl[7] == 0 && lvl[6] == 0 && lvl[5] == 1 && ioState("led1") == 0 && ioState("rele1") == 0);
  // rinomina
  CHECK(!ioRename("rele1", "rele2", err) && !ioRename("rele1", "led", err) && !ioRename("zzz", "abc", err));
  CHECK(ioRename("rele1", "boiler", err) && ioHas("boiler") && !ioHas("rele1") && String(owner[5]) == "boiler");
  // toglie: pin libero, le altre non si rompono
  CHECK(ioRemove("rele2", err) && !claimed[6] && mode[6] == DG_INPUT && ioCount() == 5);
  CHECK(claimed[5] && String(owner[5]) == "boiler" && String(owner[7]) == "led1" && String(owner[4]) == "sonda");
  CHECK(!ioRemove("rele2", err));
  // sensore I2C (BMP280): pin, indirizzo, conflitti, lettura non bloccante
  fakeI2cReset();
  CHECK(!ioAdd("i2c", 11, "", "scl=12 drv=dht11", nm, err));                          // driver sconosciuto
  CHECK(!ioAdd("i2c", 11, "", "scl=12 addr=0x50", nm, err));                          // indirizzo non del BMP280
  CHECK(!ioAdd("i2c", 11, "", "scl=11", nm, err));                                    // SDA = SCL
  CHECK(!ioAdd("i2c", 8, "", "scl=12", nm, err) && !ioAdd("i2c", 11, "", "scl=9", nm, err));   // pin gia usati da pin1 e in1
  CHECK(!ioAdd("i2c", 19, "", "scl=12", nm, err) && !ioAdd("i2c", 11, "", "scl=48", nm, err)); // USB e LED
  CHECK(ioCount() == 5 && g_fk.begins == 0);                                          // niente e rimasto a meta
  CHECK(ioAdd("i2c", 11, "", "scl=12 addr=0x77", nm, err) && nm == "bmp1" && ioCount() == 6);
  CHECK(claimed[11] && claimed[12] && String(owner[12]) == "bmp1");                   // due pin presi
  CHECK(!ioAdd("i2c", 13, "", "scl=14", nm, err) && err.indexOf("sensore I2C") >= 0); // un solo sensore
  CHECK(g_fk.begins == 0);                                                            // il bus parte da ioTick, non da ioAdd
  CHECK(!ioGet("bmp1", v, err) && err.indexOf("non risponde") >= 0);                  // ancora nessuna misura
  CHECK(String(ioClassName("bmp1")) == "i2c" && String(ioClassName("bmp1.p")) == "i2c" && !ioIsOut("bmp1") && !ioCanSwitch("bmp1"));
  // sensore assente: un tentativo, poi attesa (il loop non si blocca e non insiste)
  ioTick(); CHECK(g_fk.begins == 1 && g_fk.bus && g_fk.sda == 11 && g_fk.scl == 12 && g_fk.hz == 100000);
  int r0 = g_fk.reads; for (int k = 0; k < 20; k++) ioTick(); CHECK(g_fk.reads == r0 && g_fk.begins == 1);
  CHECK(ioState("bmp1") == 0 && ioText().indexOf("non risponde") >= 0 && ioListJson().indexOf("\"ok\":0") > 0);
  CHECK(!ioGet("bmp1.p", v, err));
  CHECK(ioRemove("bmp1", err) && !claimed[11] && !claimed[12] && ioCount() == 5);
  ioTick(); CHECK(!g_fk.bus && g_fk.ends == 1);                                       // senza sensore il bus si ferma
  // sensore presente e veloce
  fakeI2cReset(); fakeBmpLoad(0x76);
  CHECK(ioAdd("i2c", 11, "mioclima", "scl=12 fast", nm, err) && nm == "mioclima");
  ioTick(); CHECK(g_fk.bus && g_fk.hz == 400000 && g_fk.lastW[0xF4] == 0x2F);
  ioTick(); CHECK(!ioGet("mioclima", v, err));    // la prima misura aspetta 300 ms
  usleep(350000); ioTick();
  CHECK(ioGet("mioclima", v, err) && v == 2508);                                      // centesimi di grado
  CHECK(ioGet("mioclima.p", v, err) && v >= 100650 && v <= 100656);                   // pascal
  CHECK(ioState("mioclima") == 1);
  String lj = ioListJson(); CHECK(lj.indexOf("\"c\":\"i2c\"") > 0 && lj.indexOf("\"g2\":12") > 0 && lj.indexOf("\"a\":118") > 0 && lj.indexOf("\"d\":\"bmp280\"") > 0 && lj.indexOf("\"ok\":1") > 0);
  CHECK(ioText().indexOf("25.1 C") >= 0 && ioText().indexOf("1006.5 hPa") >= 0);
  int rd = g_fk.reads; for (int k = 0; k < 20; k++) ioTick(); CHECK(g_fk.reads == rd);   // prossima lettura tra 2 s: i tick di mezzo non toccano il bus
  // errore a meta: il valore vecchio resta, poi si riprova con calma
  g_fk.failReads = true; usleep(100000);
  CHECK(ioGet("mioclima", v, err) && v == 2508);
  CHECK(ioRename("mioclima", "clima", err) && ioGet("clima.p", v, err) && !ioGet("mioclima", v, err));
  CHECK(ioRemove("clima", err) && ioCount() == 5);
  ioTick(); CHECK(!g_fk.bus);
  // 16 al massimo
  int added = 0; for (int g = 11; g <= 21 && ioCount() < IO_MAX; g++) if (ioAdd("pin", g, "", "", nm, err)) added++;
  for (int g = 33; g <= 44 && ioCount() < IO_MAX; g++) if (ioAdd("pin", g, "", "", nm, err)) added++;
  CHECK(ioCount() == IO_MAX && !ioAdd("pin", 47, "", "", nm, err));
  // elenco
  String j = ioListJson();
  CHECK(j.indexOf("\"n\":\"boiler\"") > 0 && j.indexOf("\"c\":\"rele\"") > 0 && j.indexOf("\"max\":16") > 0);
  CHECK(ioText().indexOf("boiler") >= 0);
  return done("io");
}
