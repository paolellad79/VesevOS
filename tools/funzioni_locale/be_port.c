/* VesevOS - porta di script per la scheda (sostituisce default/be_port.c).
** Niente file e niente tastiera: lo script arriva come testo, l'uscita (print) va a vos_funzioni_scrivi()
** in vos_funzioni.cpp. Berry: MIT, Copyright (c) 2018-2020 Guan Wenliang (vedi LICENSE in questa cartella). */
#include "funzioni.h"
#include "be_mem.h"
#include "be_sys.h"
#include <string.h>

void vos_funzioni_scrivi(const char *buffer, size_t length);   /* vos_funzioni.cpp */

FUNZIONI_API void be_writebuffer(const char *buffer, size_t length)
{
    vos_funzioni_scrivi(buffer, length);
}

FUNZIONI_API char* be_readstring(char *buffer, size_t size)
{
    (void)buffer; (void)size;
    return NULL;
}

/* file di script: spenti (gli script si leggono da LittleFS con il driver file, non da script) */
void* be_fopen(const char *filename, const char *modes) { (void)filename; (void)modes; return NULL; }
int be_fclose(void *hfile) { (void)hfile; return 0; }
size_t be_fwrite(void *hfile, const void *buffer, size_t length) { (void)hfile; (void)buffer; (void)length; return 0; }
size_t be_fread(void *hfile, void *buffer, size_t length) { (void)hfile; (void)buffer; (void)length; return 0; }
char* be_fgets(void *hfile, void *buffer, int size) { (void)hfile; (void)buffer; (void)size; return NULL; }
int be_fseek(void *hfile, long offset) { (void)hfile; (void)offset; return -1; }
long int be_ftell(void *hfile) { (void)hfile; return -1; }
long int be_fflush(void *hfile) { (void)hfile; return 0; }
size_t be_fsize(void *hfile) { (void)hfile; return 0; }
int be_isdir(const char *path) { (void)path; return 0; }
int be_isfile(const char *path) { (void)path; return 0; }
int be_isexist(const char *path) { (void)path; return 0; }
char* be_getcwd(char *buf, size_t size) { (void)buf; (void)size; return NULL; }
int be_chdir(const char *path) { (void)path; return -1; }
int be_mkdir(const char *path) { (void)path; return -1; }
int be_unlink(const char *filename) { (void)filename; return -1; }
int be_dirfirst(bdirinfo *info, const char *path) { (void)info; (void)path; return -1; }
int be_dirnext(bdirinfo *info) { (void)info; return -1; }
int be_dirclose(bdirinfo *info) { (void)info; return 0; }
