/* VesevOS - driver in ELF per il sensore BMP280 (I2C). Esempio del formato .vde (docs/progetto/formato-vde.md).
   Comandi (vedi bmp280.vde.txt):  bmp id (Operatore)   bmp read (Operatore)   bmp init (Admin: scrive i registri del sensore)
   Widget: argv[1] = "widget" -> st=ok, v=temperatura, s=pressione.
   Il modulo non sceglie ne conosce i pin: parla al driver NOMINATO (argv[0]); i pin li ha dati l'utente quando ha installato il driver.
   Compensazione: formule a 32 bit del costruttore (nessuna divisione a 64 bit). Valore restituito: 0 = ok, altro = errore. */
#include "vde_util.h"

extern int  vos_i2c_probe(const char *drv);
extern int  vos_i2c_read(const char *drv, int reg, void *buf, int n);
extern int  vos_i2c_write(const char *drv, int reg, const void *buf, int n);

#define R_ID   0xD0
#define R_CAL  0x88
#define R_CTRL 0xF4
#define R_CONF 0xF5
#define R_DATA 0xF7

static unsigned short u16(const unsigned char *b) { return (unsigned short)(b[0] | (b[1] << 8)); }
static short s16(const unsigned char *b) { return (short)u16(b); }

typedef struct {
    unsigned short T1, P1;
    short T2, T3, P2, P3, P4, P5, P6, P7, P8, P9;
} cal_t;

/* Misura: centesimi di grado e pascal. 0 = ok, 1 = il sensore non risponde, 2 = non e un BMP280, 3 = non configurato (bmp init) */
static int misura(const char *drv, int *t100, unsigned *pa)
{
    unsigned char id = 0, raw[24], d[6];
    if (vos_i2c_read(drv, R_ID, &id, 1) != 1) {
        return 1;
    }
    if (id != 0x58 && id != 0x56 && id != 0x57 && id != 0x60) {
        return 2;
    }
    if (vos_i2c_read(drv, R_CAL, raw, 24) != 24 || vos_i2c_read(drv, R_DATA, d, 6) != 6) {
        return 1;
    }
    cal_t c;
    c.T1 = u16(raw);
    c.T2 = s16(raw + 2);
    c.T3 = s16(raw + 4);
    c.P1 = u16(raw + 6);
    c.P2 = s16(raw + 8);
    c.P3 = s16(raw + 10);
    c.P4 = s16(raw + 12);
    c.P5 = s16(raw + 14);
    c.P6 = s16(raw + 16);
    c.P7 = s16(raw + 18);
    c.P8 = s16(raw + 20);
    c.P9 = s16(raw + 22);
    int adcP = ((int)d[0] << 12) | ((int)d[1] << 4) | (d[2] >> 4);
    int adcT = ((int)d[3] << 12) | ((int)d[4] << 4) | (d[5] >> 4);
    if (adcT == 0x80000 || adcP == 0x80000) {
        return 3;
    }
    int v1 = (((adcT >> 3) - ((int)c.T1 << 1)) * (int)c.T2) >> 11;
    int dd = (adcT >> 4) - (int)c.T1;
    int v2 = (((dd * dd) >> 12) * (int)c.T3) >> 14;
    int tFine = v1 + v2;
    *t100 = (tFine * 5 + 128) >> 8;
    int p1 = (tFine >> 1) - 64000;
    int p2 = (((p1 >> 2) * (p1 >> 2)) >> 11) * (int)c.P6;
    p2 = p2 + ((p1 * (int)c.P5) << 1);
    p2 = (p2 >> 2) + ((int)c.P4 << 16);
    p1 = (((c.P3 * (((p1 >> 2) * (p1 >> 2)) >> 13)) >> 3) + (((int)c.P2 * p1) >> 1)) >> 18;
    p1 = ((32768 + p1) * (int)c.P1) >> 15;
    if (p1 == 0) {
        return 1;
    }
    unsigned p = ((unsigned)(1048576 - adcP) - (unsigned)(p2 >> 12)) * 3125u;
    if (p < 0x80000000u) {
        p = (p << 1) / (unsigned)p1;
    } else {
        p = (p / (unsigned)p1) * 2;
    }
    p1 = ((int)c.P9 * (int)(((p >> 3) * (p >> 3)) >> 13)) >> 12;
    p2 = ((int)(p >> 2) * (int)c.P8) >> 13;
    *pa = (unsigned)((int)p + ((p1 + p2 + (int)c.P7) >> 4));
    return 0;
}

static int errore(int e)
{
    vos_print(e == 1 ? "sensore non risponde" : e == 2 ? "non e un BMP280" : "sensore non configurato: bmp init (Admin)");
    return e;
}

int main(int argc, char *argv[])
{
    riga_t r;
    int t = 0;
    unsigned pa = 0;
    if (argc < 2) {
        vos_print("Uso: bmp id|read|init");
        return 20;
    }
    const char *drv = argv[0];
    const char *sub = argv[1];
    if (stringhe_uguali(sub, "id")) {
        unsigned char id = 0;
        if (vos_i2c_read(drv, R_ID, &id, 1) != 1) {
            return errore(1);
        }
        riga_inizia(&r);
        aggiungi_testo(&r, "chip id ");
        aggiungi_num(&r, id);
        vos_print(r.b);
        return 0;
    }
    if (stringhe_uguali(sub, "init")) {
        unsigned char conf = 0xA0, ctrl = 0x2F;      /* pausa 1 s, filtro spento; temperatura x1, pressione x4, misura continua */
        if (vos_i2c_write(drv, R_CONF, &conf, 1) < 0 || vos_i2c_write(drv, R_CTRL, &ctrl, 1) < 0) {
            vos_print("scrittura rifiutata (serve l'Admin) o sensore non risponde");
            return 1;
        }
        vos_print("sensore configurato");
        return 0;
    }
    if (stringhe_uguali(sub, "read")) {
        int e = misura(drv, &t, &pa);
        if (e) {
            return errore(e);
        }
        riga_inizia(&r);
        aggiungi_cent(&r, t);
        aggiungi_testo(&r, " C, ");
        aggiungi_num(&r, (int)(pa / 100));
        aggiungi_testo(&r, ".");
        aggiungi_num(&r, (int)((pa % 100) / 10));
        aggiungi_testo(&r, " hPa");
        vos_print(r.b);
        return 0;
    }
    if (stringhe_uguali(sub, "widget")) {
        int e = misura(drv, &t, &pa);
        if (e) {
            vos_print("st=off");
            vos_print(e == 3 ? "v=Non configurato" : "v=Non risponde");
            return 0;
        }
        vos_print("st=ok");
        riga_inizia(&r);
        aggiungi_testo(&r, "v=");
        aggiungi_cent(&r, t);
        aggiungi_testo(&r, " C");
        vos_print(r.b);
        riga_inizia(&r);
        aggiungi_testo(&r, "s=");
        aggiungi_num(&r, (int)(pa / 100));
        aggiungi_testo(&r, ".");
        aggiungi_num(&r, (int)((pa % 100) / 10));
        aggiungi_testo(&r, " hPa");
        vos_print(r.b);
        return 0;
    }
    vos_print("Uso: bmp id|read|init");
    return 20;
}
