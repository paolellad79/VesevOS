Berry (linguaggio di script) - copia usata da VesevOS
=====================================================
Origine : https://github.com/berry-lang/berry  (Berry 1.1.0, commit f333033 del 9 ottobre 2026)
Licenza : MIT, Copyright (c) 2018-2020 Guan Wenliang  (testo completo in LICENSE, qui accanto)
Usato da: VesevOS/src/packages/vos_berry.cpp (package VOS_WITH_BERRY)

Cosa e cambiato rispetto all'originale (tutto in 3 file, le copie master stanno in tools/berry_local/):
  src/berry_conf.h  configurazione ridotta: interi a 32 bit, numeri a precisione singola, niente file, niente bytecode su file,
                    niente libreria dinamica; moduli ammessi: string, json, math, gc; memoria da vos_berry_malloc/free/realloc.
  src/be_port.c     niente file e niente tastiera; l'uscita (print) va a vos_berry_write().
  src/be_modtab.c   tabella dei moduli: solo string, json, math, gc (niente os, sys, debug, introspect, solidify, global).
Tolti : be_repl.c / be_repl.h (la console di Berry non serve).
Tabelle generate (generate/): le crea tools/coc di Berry; si rifanno con  python3 tools/mkberry.py <copia di berry>.
Per aggiornare Berry: scaricare la nuova versione, rilanciare tools/mkberry.py, rifare tools/hosttest/run.sh (test_berry) e la prova sulla scheda.
