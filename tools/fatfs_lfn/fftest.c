// Prova di FatFs di ESP-IDF (stesse opzioni di Arduino-ESP32 3.3.12) su un disco in RAM: nomi lunghi, spazi, maiuscole, UTF-8, 255 caratteri.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ff.h"
#include "diskio.h"
#define SECT 512
#define NSEC (64*1024*2)          /* 64 MB */
static unsigned char* disk;
DSTATUS disk_status(BYTE p) { return 0; }
DSTATUS disk_initialize(BYTE p) { return 0; }
DRESULT disk_read(BYTE p, BYTE* b, LBA_t s, UINT c) { memcpy(b, disk + (size_t)s * SECT, (size_t)c * SECT); return RES_OK; }
DRESULT disk_write(BYTE p, const BYTE* b, LBA_t s, UINT c) { memcpy(disk + (size_t)s * SECT, b, (size_t)c * SECT); return RES_OK; }
DRESULT disk_ioctl(BYTE p, BYTE cmd, void* buf) {
  switch (cmd) { case CTRL_SYNC: return RES_OK; case GET_SECTOR_COUNT: *(LBA_t*)buf = NSEC; return RES_OK; case GET_SECTOR_SIZE: *(WORD*)buf = SECT; return RES_OK; case GET_BLOCK_SIZE: *(DWORD*)buf = 1; return RES_OK; }
  return RES_PARERR;
}
DWORD get_fattime(void) { return (2026 - 1980) << 25 | 10 << 21 | 15 << 16; }
PARTITION VolToPart[FF_VOLUMES] = {{0, 0}};
int ff_mutex_create(int v) { return 1; } void ff_mutex_delete(int v) {} int ff_mutex_take(int v) { return 1; } void ff_mutex_give(int v) {}
void* ff_memalloc(unsigned n) { return malloc(n); } void ff_memfree(void* p) { free(p); }
static FATFS fs;
static int fails = 0;
#define CHECK(c, what) do { if (c) printf("  ok  %s\n", what); else { printf("  KO  %s\n", what); fails++; } } while (0)
static int make(const char* path, const char* txt) { FIL f; FRESULT r = f_open(&f, path, FA_WRITE | FA_CREATE_ALWAYS); if (r) return r; UINT w; f_write(&f, txt, strlen(txt), &w); f_close(&f); return 0; }
static int readb(const char* path, char* out, int n) { FIL f; FRESULT r = f_open(&f, path, FA_READ); if (r) return -(int)r; UINT g = 0; f_read(&f, out, n - 1, &g); out[g] = 0; f_close(&f); return (int)g; }
int main() {
  disk = calloc(NSEC, SECT);
  printf("FF_USE_LFN=%d FF_MAX_LFN=%d FF_LFN_UNICODE=%d FF_CODE_PAGE=%d FF_FS_EXFAT=%d FF_MIN_SS=%d\n", FF_USE_LFN, FF_MAX_LFN, FF_LFN_UNICODE, FF_CODE_PAGE, FF_FS_EXFAT, FF_MIN_SS);
  BYTE work[4096]; MKFS_PARM opt = { FM_FAT32, 0, 0, 0, 0 };
  FRESULT r = f_mkfs("", &opt, work, sizeof work); printf("mkfs FAT32: %d\n", r); r = f_mount(&fs, "", 1); printf("mount: %d, tipo fs=%d (3=FAT32)\n", r, fs.fs_type);
  char b[300]; char nm[300];
  // spazi e punti
  CHECK(make("/dati/x", "") != 0, "cartella mancante: errore (atteso)");
  CHECK(f_mkdir("/dati") == 0, "mkdir /dati");
  CHECK(make("/dati/log 1 (2).txt", "spazi") == 0 && readb("/dati/log 1 (2).txt", b, sizeof b) == 5, "nome con spazi e parentesi");
  CHECK(make("/a.b.c.txt", "punti") == 0 && readb("/a.b.c.txt", b, sizeof b) == 5, "piu punti nel nome");
  // lunghezza: nome da 255 caratteri (il massimo di LFN) e da 256
  memset(nm, 'n', 255); nm[0] = '/'; nm[255] = 0;   /* '/' + 254 */
  CHECK(make(nm, "l254") == 0 && readb(nm, b, sizeof b) == 4, "nome di 254 caratteri");
  memset(nm, 'n', 256); nm[0] = '/'; nm[256] = 0;   /* '/' + 255 */
  CHECK(make(nm, "l255") == 0, "nome di 255 caratteri (limite)");
  memset(nm, 'n', 257); nm[0] = '/'; nm[257] = 0;   /* '/' + 256 */
  CHECK(make(nm, "l256") != 0, "nome di 256 caratteri: rifiutato");
  // maiuscole/minuscole
  CHECK(make("/Maiusc.TXT", "M") == 0 && readb("/maiusc.txt", b, sizeof b) == 1 && readb("/MAIUSC.TXT", b, sizeof b) == 1, "maiuscole e minuscole: stesso file (FAT non distingue)");
  { FILINFO fi; CHECK(f_stat("/maiusc.txt", &fi) == 0, "stat funziona con altre maiuscole"); printf("      nome salvato: %s\n", fi.fname); }
  CHECK(make("/MAIUSC.txt", "N") == 0 && readb("/Maiusc.TXT", b, sizeof b) == 1 && b[0] == 'N', "ricreare con altre maiuscole: sovrascrive lo stesso file");
  // UTF-8 (accenti, euro, cinese, emoji fuori dal piano base)
  const char* u[] = { "/caff\xc3\xa8.txt", "/\xe2\x82\xac" "uro.txt", "/\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e.txt", "/emoji\xf0\x9f\x98\x80.txt", "/\xc3\x80\xc3\x89.TXT" };
  const char* un[] = { "accento", "euro", "cinese", "emoji", "accenti maiuscoli" };
  for (int i = 0; i < 5; i++) { int w = make(u[i], un[i]); int rd = w == 0 ? readb(u[i], b, sizeof b) : -99; char m[80]; snprintf(m, sizeof m, "UTF-8: %s (crea=%d, legge=%d)", un[i], w, rd); CHECK(w == 0 && rd == (int)strlen(un[i]), m); }
  CHECK(readb("/CAFF\xc3\x88.TXT", b, sizeof b) > 0, "accento: maiuscolo/minuscolo trattati come uguali (\xc3\xa8 = \xc3\x88)");
  CHECK(readb("/\xc3\xa0\xc3\xa9.txt", b, sizeof b) > 0, "accenti: le minuscole trovano il file creato in maiuscolo");
  // caratteri vietati da FAT
  const char* bad[] = { "/a:b.txt", "/a*b.txt", "/a?b.txt", "/a\"b.txt", "/a<b.txt", "/a>b.txt", "/a|b.txt", "/a\\b.txt" };
  for (int i = 0; i < 8; i++) { char m[60]; snprintf(m, sizeof m, "carattere vietato rifiutato: %s", bad[i] + 1); CHECK(make(bad[i], "x") != 0, m); }
  CHECK(make("/finale. ", "x") == 0 || 1, "(nome che finisce con spazio/punto: informativo)");
  { FILINFO fi; r = f_stat("/finale", &fi); printf("      \"finale. \" salvato come: %s (stat=%d)\n", r == 0 ? fi.fname : "-", r); }
  // elenco: lunghezze dei nomi reali
  FF_DIR d; FILINFO fi; int n = 0, maxl = 0; f_opendir(&d, "/"); while (f_readdir(&d, &fi) == 0 && fi.fname[0]) { n++; if (strlen(fi.fname) < 40) printf("      voce: [%s] (8.3: %s)\n", fi.fname, fi.altname); int l = (int)strlen(fi.fname); if (l > maxl) maxl = l; } f_closedir(&d);
  printf("      voci in radice: %d, nome piu lungo in byte: %d\n", n, maxl);
  printf("%s: %d fallite\n", fails ? "ESITO KO" : "ESITO OK", fails);
  return fails != 0;
}
