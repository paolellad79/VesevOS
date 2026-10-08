// Mini strumento di prova: CHECK(condizione) e done("nome")
#pragma once
#include <stdio.h>
static int g_fail = 0, g_n = 0;
#define CHECK(c) do { g_n++; if (!(c)) { g_fail++; printf("  KO riga %d: %s\n", __LINE__, #c); } } while (0)
static int done(const char* name) { printf("%s: %d prove, %d fallite\n", name, g_n, g_fail); return g_fail ? 1 : 0; }
