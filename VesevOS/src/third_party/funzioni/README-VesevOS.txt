Interprete delle funzioni VesevOS (linguaggio di script) - copia usata da VesevOS
===============================================================================
Origine : https://github.com/berry-lang/berry  (Berry 1.1.0, commit f333033 del 9 ottobre 2026)
Licenza : MIT, Copyright (c) 2018-2020 Guan Wenliang  (testo completo in LICENSE, qui accanto)
Usato da: VesevOS/src/packages/vos_funzioni.cpp (package VOS_WITH_FUNZIONI)

Cosa e cambiato rispetto all'originale (le copie master stanno in tools/funzioni_locale/):
  src/funzioni_conf.h  configurazione ridotta: interi a 32 bit, numeri a precisione singola, niente file, niente bytecode su file,
                       niente libreria dinamica; moduli ammessi: string, json, math, gc; memoria da vos_funzioni_alloca/libera/rialloca.
  src/be_port.c        niente file e niente tastiera; l'uscita (print) va a vos_funzioni_scrivi().
  src/be_modtab.c      tabella dei moduli: solo string, json, math, gc (niente os, sys, debug, introspect, solidify, global).
  nomi                 file, macro e simboli che portavano il nome del progetto originale hanno un nome nostro
                       (funzioni.h, funzioni_conf.h, FUNZIONI_API, ...). Gli avvisi di copyright e la licenza sono rimasti intatti.
Tolti : be_repl.c / be_repl.h (la console non serve).
Tabelle generate (generate/): le crea lo strumento coc dell'originale; si rifanno con  python3 tools/mkfunzioni.py <copia dell'originale>.
Per aggiornare: scaricare la nuova versione, rilanciare tools/mkfunzioni.py, rifare tools/hosttest/run.sh (test_funzioni) e la prova sulla scheda.
