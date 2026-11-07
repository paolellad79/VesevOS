/* VesevOS - driver in ELF per il lettore microSD (SPI). Esempio del formato .vde (docs/progetto/formato-vde.md).
   Comandi (vedi sd-spi.vde.txt): sd status | ls [cartella] | read <file> | mount | unmount | write <file> <testo> | append <file> <testo> | rm <file> | mkdir <cartella>
   Lo stato della Home (widget) NON passa da qui: lo tiene il firmware (source=sd). Il montaggio e lo smontaggio li fa il firmware con le sue primitive.
   Percorsi: "foto/a.txt" o "/foto/a.txt" stanno sulla scheda (si mette /sd davanti). Valore restituito: 0 = ok, altro = errore. */
#include "vde_util.h"

extern int vos_sd_status(int *valori, int n, char *fs, int fs_max);
extern int vos_sd_mount(void);
extern int vos_sd_format(const char *parola);
extern int vos_sd_eject(void);
extern int vos_sd_read(const char *percorso, int offset, void *buf, int n);
extern int vos_sd_write(const char *percorso, const void *dati, int n, int append);
extern int vos_sd_rm(const char *percorso);
extern int vos_sd_mkdir(const char *percorso);
extern int vos_listdir(const char *percorso, int indice, char *out, int out_max, int *prossimo);

static char g_percorso[120];

/* "a/b" o "/a/b" -> "/sd/a/b" (un percorso gia sotto /sd resta com'e) */
static const char *percorso_sd(const char *p)
{
    int n = 0;
    const char *base = "/sd";
    if (!p || !p[0]) {
        return base;
    }
    if (p[0] == '/' && p[1] == 's' && p[2] == 'd' && (p[3] == 0 || p[3] == '/')) {
        base = "";
    }
    while (base[n] && n < (int)sizeof(g_percorso) - 1) {
        g_percorso[n] = base[n];
        n++;
    }
    if (p[0] != '/' && base[0] && n < (int)sizeof(g_percorso) - 1) {
        g_percorso[n++] = '/';
    }
    while (*p && n < (int)sizeof(g_percorso) - 1) {
        g_percorso[n++] = *p++;
    }
    g_percorso[n] = 0;
    return g_percorso;
}

static int fallito(const char *cosa, int e)
{
    riga_t r;
    riga_inizia(&r);
    aggiungi_testo(&r, cosa);
    aggiungi_testo(&r, ": errore ");
    aggiungi_num(&r, e);
    vos_print(r.b);
    return e < 0 ? -e : 1;
}

static int stato(void)
{
    static const char *nomi[] = { "non configurata", "montata", "non montata", "non rilevata", "formato non supportato (serve FAT32)" };
    int v[6];
    char fs[8];
    riga_t r;
    if (vos_sd_status(v, 6, fs, (int)sizeof(fs)) < 0) {
        return fallito("stato", -1);
    }
    if (v[4]) {                                 /* formattazione in corso: v[5] = secondi trascorsi */
        riga_inizia(&r);
        aggiungi_testo(&r, "formattazione in corso, ");
        aggiungi_num(&r, v[5] / 60);
        aggiungi_testo(&r, " min ");
        aggiungi_num(&r, v[5] % 60);
        aggiungi_testo(&r, " s");
        vos_print(r.b);
        return 0;
    }
    riga_inizia(&r);
    aggiungi_testo(&r, "microSD: ");
    aggiungi_testo(&r, nomi[v[0] >= 0 && v[0] <= 4 ? v[0] : 0]);
    vos_print(r.b);
    if (v[0] == 1) {
        riga_inizia(&r);
        aggiungi_testo(&r, fs);
        aggiungi_testo(&r, ", ");
        aggiungi_num(&r, (v[1] - v[2]) / 1024);
        aggiungi_testo(&r, " MB liberi su ");
        aggiungi_num(&r, v[1] / 1024);
        aggiungi_testo(&r, " MB, ");
        aggiungi_num(&r, v[3]);
        aggiungi_testo(&r, " kHz");
        vos_print(r.b);
    }
    return 0;
}

/* Una riga per voce: "nome/" per le cartelle, "nome  123" (byte) per i file */
static int elenca(const char *cartella)
{
    static char out[700];
    int prossimo = 0, indice = 0;
    while (indice >= 0) {
        int n = vos_listdir(percorso_sd(cartella), indice, out, (int)sizeof(out), &prossimo);
        if (n < 0) {
            return fallito("elenco", n);
        }
        char *p = out;
        while (*p) {
            char *fine = p;
            while (*fine && *fine != '\n') {
                fine++;
            }
            char c = *fine;
            *fine = 0;
            riga_t r;
            riga_inizia(&r);
            char *num = p + 2;                         /* "D 0 nome" o "F 123 nome" */
            char *nome = num;
            while (*nome && *nome != ' ') {
                nome++;
            }
            if (*nome) {
                *nome++ = 0;
            }
            aggiungi_testo(&r, nome);
            if (p[0] == 'D') {
                aggiungi_testo(&r, "/");
            } else {
                aggiungi_testo(&r, "  ");
                aggiungi_testo(&r, num);
            }
            vos_print(r.b);
            if (!c) {
                break;
            }
            p = fine + 1;
        }
        indice = n > 0 ? prossimo : -1;
    }
    return 0;
}

static int leggi(const char *file)
{
    static char buf[200];
    int n = vos_sd_read(percorso_sd(file), 0, buf, (int)sizeof(buf) - 1);
    if (n < 0) {
        return fallito("lettura", n);
    }
    buf[n] = 0;
    vos_print(buf);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        vos_print("Uso: sd status|ls|read|mount|unmount|format|write|append|rm|mkdir");
        return 20;
    }
    const char *sub = argv[1];
    const char *a1 = argc > 2 ? argv[2] : "";
    const char *a2 = argc > 3 ? argv[3] : "";
    int e = 0;
    if (stringhe_uguali(sub, "status")) {
        return stato();
    }
    if (stringhe_uguali(sub, "ls")) {
        return elenca(a1);
    }
    if (stringhe_uguali(sub, "read")) {
        return a1[0] ? leggi(a1) : (vos_print("Uso: sd read <file>"), 20);
    }
    if (stringhe_uguali(sub, "mount")) {
        e = vos_sd_mount();
        if (e < 0) {
            return fallito("montaggio", e);
        }
        vos_print("microSD montata");
        return 0;
    }
    if (stringhe_uguali(sub, "unmount")) {
        e = vos_sd_eject();
        if (e < 0) {
            return fallito("smontaggio", e);
        }
        vos_print("microSD smontata");
        return 0;
    }
    if (stringhe_uguali(sub, "format")) {
        if (!stringhe_uguali(a1, "FORMATTA")) {
            vos_print("ATTENZIONE: cancella TUTTO sulla microSD (FAT32)");
            vos_print("Per confermare: sd format FORMATTA");
            return 20;
        }
        e = vos_sd_format("FORMATTA");
        if (e < 0) {
            return fallito("formattazione", e);
        }
        vos_print("Formattazione avviata: il risultato si vede con sd status");
        return 0;
    }
    if (stringhe_uguali(sub, "write") || stringhe_uguali(sub, "append")) {
        if (!a1[0]) {
            vos_print("Uso: sd write|append <file> <testo>");
            return 20;
        }
        e = vos_sd_write(percorso_sd(a1), a2, (int)strlen(a2), stringhe_uguali(sub, "append"));
        if (e < 0) {
            return fallito("scrittura", e);
        }
        vos_print("fatto");
        return 0;
    }
    if (stringhe_uguali(sub, "rm") || stringhe_uguali(sub, "mkdir")) {
        if (!a1[0]) {
            vos_print("Uso: sd rm|mkdir <percorso>");
            return 20;
        }
        e = stringhe_uguali(sub, "rm") ? vos_sd_rm(percorso_sd(a1)) : vos_sd_mkdir(percorso_sd(a1));
        if (e < 0) {
            return fallito(sub, e);
        }
        vos_print("fatto");
        return 0;
    }
    vos_print("Uso: sd status|ls|read|mount|unmount|format|write|append|rm|mkdir");
    return 20;
}
