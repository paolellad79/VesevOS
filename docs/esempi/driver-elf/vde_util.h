/* VesevOS - piccoli aiuti per i moduli dei driver ELF (.vde): niente libc oltre memcpy/memset/strlen, quindi i testi si compongono a mano.
   Una riga si costruisce in un buffer con aggiungi_testo / aggiungi_num / aggiungi_cent e si scrive con vos_print() (una riga per chiamata). */
#ifndef VDE_UTIL_H
#define VDE_UTIL_H
#include <string.h>

extern int vos_print(const char *testo);

typedef struct { char b[96]; int n; } riga_t;

static void riga_inizia(riga_t *r) { r->n = 0; r->b[0] = 0; }

static void aggiungi_testo(riga_t *r, const char *t)
{
    while (*t && r->n < (int)sizeof(r->b) - 1) {
        r->b[r->n++] = *t++;
    }
    r->b[r->n] = 0;
}

/* intero con segno */
static void aggiungi_num(riga_t *r, int v)
{
    char tmp[12];
    int i = 0;
    unsigned u = v < 0 ? (unsigned)(-v) : (unsigned)v;
    if (v < 0) {
        aggiungi_testo(r, "-");
    }
    do {
        tmp[i++] = (char)('0' + u % 10);
        u /= 10;
    } while (u && i < 11);
    while (i > 0 && r->n < (int)sizeof(r->b) - 1) {
        r->b[r->n++] = tmp[--i];
    }
    r->b[r->n] = 0;
}

/* centesimi come 21.53 */
static void aggiungi_cent(riga_t *r, int cent)
{
    int a = cent < 0 ? -cent : cent;
    if (cent < 0) {
        aggiungi_testo(r, "-");
    }
    aggiungi_num(r, a / 100);
    aggiungi_testo(r, ".");
    if (a % 100 < 10) {
        aggiungi_testo(r, "0");
    }
    aggiungi_num(r, a % 100);
}

static int stringhe_uguali(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}
#endif
