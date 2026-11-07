#!/bin/bash
# Prova sul computer della lettura degli elenchi degli aggiornamenti e dell'orario (senza Arduino, senza rete).
# Uso: tools/hosttest/prova_aggiorna.sh
set -e
R=$(cd "$(dirname "$0")/../.." && pwd); O=$(mktemp -d)
cat > "$O/prova.cpp" <<'CPP'
#include <stdio.h>
#include <string.h>
#include "packages/vos_aggiorna_dati.h"
#include "core/vos_pacchetto.h"
#include "core/vos_updorder.h"
#include "rec_riserva_scelta.h"

static int falliti = 0;
static void vero(bool c, const char* nome) { if (!c) { printf("KO %s\n", nome); falliti++; } }

static const char* SHA = "2bce5a99e14c54912d8772f966cf4d7ecc77d420c910c604ddaf96d56962cf22";

static bool leggi(const char* t, AggDisponibile& d) { return aggLeggiUltima(t, strlen(t), d); }

int main() {
  // versioni del firmware
  vero(pacConfrontaFirmware("1.8.18", "1.8.17a") == 1, "1.8.18 > 1.8.17a");
  vero(pacConfrontaFirmware("1.8.17a", "1.8.17") == 1, "1.8.17a > 1.8.17");
  vero(pacConfrontaFirmware("1.8.17", "1.8.17a") == -1, "1.8.17 < 1.8.17a");
  vero(pacConfrontaFirmware("1.8.17a", "1.8.17b") == -1, "a < b");
  vero(pacConfrontaFirmware("1.10.0", "1.9.9") == 1, "1.10 > 1.9");
  vero(pacConfrontaFirmware("1.8", "1.8.0") == 0, "1.8 = 1.8.0");
  vero(pacConfrontaFirmware("1.8.18", "1.8.18") == 0, "uguali");
  vero(pacConfrontaFirmware("", "1.8") == PAC_VERSIONE_ILLEGGIBILE, "vuota");
  vero(pacConfrontaFirmware("beta", "1.8") == PAC_VERSIONE_ILLEGGIBILE, "lettere");
  vero(pacConfrontaFirmware("1.8-rc", "1.8") == PAC_VERSIONE_ILLEGGIBILE, "trattino");
  vero(pacConfrontaFirmware("1.8.", "1.8") == PAC_VERSIONE_ILLEGGIBILE, "punto finale");
  vero(pacConfrontaFirmware("a", "1.8") == PAC_VERSIONE_ILLEGGIBILE, "solo lettera");

  // versione minima del recovery per l'installazione automatica
  vero(pacConfrontaFirmware("0.5.3", "0.5.3") == 0, "recovery uguale");
  vero(pacConfrontaFirmware("0.5.2", "0.5.3") == -1, "recovery vecchio");
  vero(pacConfrontaFirmware("0.5.10", "0.5.3") == 1, "recovery 0.5.10");
  vero(pacConfrontaFirmware("", "0.5.3") == PAC_VERSIONE_ILLEGGIBILE, "recovery senza marchio");

  // tentativi dell'installazione automatica
  vero(aggSpazioRiserva(2400, 2359296), "riserva: entra");
  vero(!aggSpazioRiserva(2300, 2359296), "riserva: non entra");
  vero(risScegli(RIS_VUOTA, 0, true, true) == RIS_NIENTE, "riserva assente");
  vero(risScegli(RIS_ARMATA, 0, false, false) == RIS_NIENTE, "firmware nuovo sano");
  vero(risScegli(RIS_ARMATA, 0, false, true) == RIS_RIPROVA, "scartato: secondo avvio");
  vero(risScegli(RIS_ARMATA, 1, false, true) == RIS_RIPRISTINA, "scartato due volte: riserva");
  vero(risScegli(RIS_ARMATA, 0, true, false) == RIS_RIPRISTINA, "app rotta: riserva");
  vero(risScegli(RIS_USATA, 0, true, false) == RIS_CHIUDI, "ripristino interrotto: niente cicli");
  vero(aggTentativoAmmesso("", 0, "1.8.19"), "nessun tentativo");
  vero(aggTentativoAmmesso("1.8.19", 2, "1.8.19"), "terzo tentativo");
  vero(!aggTentativoAmmesso("1.8.19", 3, "1.8.19"), "tentativi finiti");
  vero(!aggTentativoAmmesso("1.8.19", 9, "1.8.19"), "resta fermo");
  vero(aggTentativoAmmesso("1.8.19", 3, "1.8.20"), "versione nuova riparte");
  vero(aggTentativiDaAzzerare("1.8.19", "1.8.19"), "installata: si azzera");
  vero(aggTentativiDaAzzerare("1.8.19", "1.8.20"), "superata: si azzera");
  vero(!aggTentativiDaAzzerare("1.8.19", "1.8.18"), "non installata: resta");
  vero(!aggTentativiDaAzzerare("", "1.8.18"), "niente da azzerare");

  // pin della microSD accettati dal recovery
  int pinBuoni[4] = {10, 11, 12, 13};
  vero(updPinSdOk(pinBuoni), "pin buoni");
  int pinFlash[4] = {10, 11, 12, 29};
  vero(!updPinSdOk(pinFlash), "pin della flash");
  int pinUsb[4] = {10, 19, 12, 13};
  vero(!updPinSdOk(pinUsb), "pin della USB");
  int pinDoppi[4] = {10, 11, 11, 13};
  vero(!updPinSdOk(pinDoppi), "pin doppi");
  int pinFuori[4] = {10, 11, 12, 49};
  vero(!updPinSdOk(pinFuori), "pin fuori scala");
  int pinNeg[4] = {-1, 11, 12, 13};
  vero(!updPinSdOk(pinNeg), "pin negativo");

  // firmware scaricato sulla microSD: intestazione e marchio a pezzi
  uint8_t testa[16] = {0xE9};
  testa[12] = 0x09;
  vero(aggTestaFirmware(testa, sizeof testa), "testa buona");
  vero(!aggTestaFirmware(testa, 8), "testa corta");
  testa[12] = 0x05;
  vero(!aggTestaFirmware(testa, sizeof testa), "altro chip");
  size_t pos = 0;
  vero(!aggMarchio((const uint8_t*)"xx{FW:Ves", 9, pos), "marchio a meta");
  vero(aggMarchio((const uint8_t*)"evOS}yy", 7, pos), "marchio tra due blocchi");
  vero(aggMarchio((const uint8_t*)"zz", 2, pos), "marchio resta trovato");
  pos = 0;
  vero(!aggMarchio((const uint8_t*)"{FW:VesX", 8, pos), "marchio rotto");
  vero(aggMarchio((const uint8_t*)"{{FW:VesevOS}", 13, pos), "marchio dopo una falsa partenza");

  // aggiorna/latest.json
  char t[900];
  AggDisponibile d;
  snprintf(t, sizeof t, "{\"schema\":1,\"altro\":[1,2],\"firmware\":{\"version\":\"1.8.19\",\"size\":2380880,\"sha256\":\"%s\",\"url\":\"https://raw.githubusercontent.com/x/y.bin\",\"channel\":\"official\",\"publisher\":\"Domenico\",\"date\":\"2026-11-08\"},\"recovery\":{\"version\":\"0.5.2\"}}", SHA);
  vero(leggi(t, d), "elenco buono");
  vero(!strcmp(d.ver, "1.8.19") && d.dim == 2380880 && !strcmp(d.canale, "official") && !strcmp(d.verRec, "0.5.2") && !strcmp(d.data, "2026-11-08"), "campi");
  snprintf(t, sizeof t, "{\"schema\":2,\"firmware\":{\"version\":\"1.8.19\",\"size\":1,\"sha256\":\"%s\",\"url\":\"https://x/y\"}}", SHA);
  vero(!leggi(t, d), "schema sconosciuto");
  snprintf(t, sizeof t, "{\"schema\":1,\"firmware\":{\"version\":\"1.8.19\",\"size\":0,\"sha256\":\"%s\",\"url\":\"https://x/y\"}}", SHA);
  vero(!leggi(t, d), "dimensione zero");
  snprintf(t, sizeof t, "{\"schema\":1,\"firmware\":{\"version\":\"1.8.19\",\"size\":-5,\"sha256\":\"%s\",\"url\":\"https://x/y\"}}", SHA);
  vero(!leggi(t, d), "dimensione negativa");
  snprintf(t, sizeof t, "{\"schema\":1,\"firmware\":{\"version\":\"1.8.19\",\"size\":1,\"sha256\":\"%s\",\"url\":\"http://x/y\"}}", SHA);
  vero(!leggi(t, d), "non https");
  snprintf(t, sizeof t, "{\"schema\":1,\"firmware\":{\"version\":\"1.8.19\",\"size\":1,\"sha256\":\"%s\",\"url\":\"https://x/a b\"}}", SHA);
  vero(!leggi(t, d), "spazio nell'indirizzo");
  vero(!leggi("{\"schema\":1,\"firmware\":{\"version\":\"1.8.19\",\"size\":1,\"sha256\":\"abc\",\"url\":\"https://x/y\"}}", d), "impronta corta");
  snprintf(t, sizeof t, "{\"schema\":1,\"firmware\":{\"version\":\"nuova\",\"size\":1,\"sha256\":\"%s\",\"url\":\"https://x/y\"}}", SHA);
  vero(!leggi(t, d), "versione illeggibile");
  snprintf(t, sizeof t, "{\"schema\":1,\"firmware\":{\"size\":1,\"sha256\":\"%s\",\"url\":\"https://x/y\"}}", SHA);
  vero(!leggi(t, d), "versione mancante");
  snprintf(t, sizeof t, "{\"schema\":1,\"firmware\":{\"version\":123,\"size\":1,\"sha256\":\"%s\",\"url\":\"https://x/y\"}}", SHA);
  vero(!leggi(t, d), "versione numero");
  snprintf(t, sizeof t, "{\"schema\":1,\"firmware\":{\"version\":\"1.8.19\",\"size\":1,\"sha256\":\"%s\",\"url\":\"https://x/%0220d\"}}", SHA, 0);
  vero(!leggi(t, d), "indirizzo troppo lungo");
  vero(!leggi("", d), "vuoto");
  vero(!leggi("non json", d), "non json");
  vero(!leggi("{\"schema\":1,\"firmware\":{\"version\":\"1.8", d), "tagliato");
  vero(!leggi("[[[[[[[[[[[[[[[[[[[[[[[[[[[[1]]]]]]]]]]]]]]]]]]]]]]]]]]]]", d), "troppo annidato");

  // lettore piccolo: escape
  snprintf(t, sizeof t, " {\"schema\":1,\n\"firmware\":{\"version\":\"1.8.19\",\"size\":1,\"sha256\":\"%s\",\"url\":\"https:\\/\\/x\\/y\",\"publisher\":\"A\\\"B\\\\C\\u0041\\u00e8\\u20ac\"}} ", SHA);
  vero(leggi(t, d), "escape letti");
  vero(!strcmp(d.url, "https://x/y") && !strcmp(d.chi, "A\"B\\CA\xc3\xa8\xe2\x82\xac"), "escape decodificati");
  snprintf(t, sizeof t, "{\"schema\":1,\"firmware\":{\"version\":\"1.8.19\",\"size\":1,\"sha256\":\"%s\",\"url\":\"https://x/y\",\"publisher\":\"a\\q\"}}", SHA);
  vero(!leggi(t, d), "escape sconosciuto");
  snprintf(t, sizeof t, "{\"schema\":1,\"firmware\":{\"version\":\"1.8.19\",\"size\":1,\"sha256\":\"%s\",\"url\":\"https://x/y\",\"publisher\":\"a\\u12\"}}", SHA);
  vero(!leggi(t, d), "\\u corto");
  snprintf(t, sizeof t, "{\"schema\":1,\"firmware\":{\"version\":\"1.8.19\",\"size\":1,\"sha256\":\"%s\",\"url\":\"https://x/y\",\"publisher\":\"a\\u0000b\"}}", SHA);
  vero(!leggi(t, d), "\\u0000 in un campo usato");
  snprintf(t, sizeof t, "{\"schema\":1,\"firmware\":{\"version\":\"1.8.19\",\"size\":1,\"sha256\":\"%s\",\"url\":\"https://x/y\",\"publisher\":\"\\ud83d\\ude00\"}}", SHA);
  vero(!leggi(t, d), "surrogate in un campo usato");
  snprintf(t, sizeof t, "{\"schema\":1,\"note\":\"\\ud83d\\ude00\\u0000\",\"firmware\":{\"version\":\"1.8.19\",\"size\":1,\"sha256\":\"%s\",\"url\":\"https://x/y\"}}", SHA);
  vero(leggi(t, d), "surrogate in un campo ignorato");
  snprintf(t, sizeof t, "{\"schema\":1,\"firmware\":{\"version\":\"1.8.19\",\"size\":1,\"sha256\":\"%s\",\"url\":\"https://x/y\",\"publisher\":\"a\nb\"}}", SHA);
  vero(!leggi(t, d), "a capo dentro una stringa");

  // lettore piccolo: valori lunghi
  snprintf(t, sizeof t, "{\"schema\":1,\"firmware\":{\"version\":\"1.8.19\",\"size\":1,\"sha256\":\"%s\",\"url\":\"https://x/y\",\"publisher\":\"%040d\"}}", SHA, 0);
  vero(!leggi(t, d), "publisher troppo lungo");
  snprintf(t, sizeof t, "{\"schema\":1,\"firmware\":{\"version\":\"1.8.19\",\"size\":1,\"sha256\":\"%s\",\"url\":\"https://x/y\",\"publisher\":\"%032d\"}}", SHA, 0);
  vero(leggi(t, d) && strlen(d.chi) == 32, "publisher lungo giusto");
  snprintf(t, sizeof t, "{\"schema\":1,\"firmware\":{\"version\":\"1.8.19\",\"size\":1,\"sha256\":\"%s\",\"url\":\"https://x/y\",\"publisher\":\"%033d\"}}", SHA, 0);
  vero(!leggi(t, d), "publisher un carattere di troppo");
  snprintf(t, sizeof t, "{\"schema\":1,\"chiave-molto-molto-lunga-che-non-conosciamo\":\"x\",\"note\":\"%0300d\",\"firmware\":{\"version\":\"1.8.19\",\"size\":1,\"sha256\":\"%s\",\"url\":\"https://x/y\"}}", 0, SHA);
  vero(leggi(t, d), "campi ignorati lunghi");
  snprintf(t, sizeof t, "{\"schema\":1,\"firmware\":{\"version\":\"1.8.19\",\"size\":4294967296,\"sha256\":\"%s\",\"url\":\"https://x/y\"}}", SHA);
  vero(!leggi(t, d), "dimensione oltre 32 bit");
  snprintf(t, sizeof t, "{\"schema\":1,\"firmware\":{\"version\":\"1.8.19\",\"size\":99999999999999999999999999,\"sha256\":\"%s\",\"url\":\"https://x/y\"}}", SHA);
  vero(!leggi(t, d), "dimensione enorme");
  snprintf(t, sizeof t, "{\"schema\":1,\"firmware\":{\"version\":\"1.8.19\",\"size\":4294967295,\"sha256\":\"%s\",\"url\":\"https://x/y\"}}", SHA);
  vero(leggi(t, d) && d.dim == 4294967295u, "dimensione massima");

  // lettore piccolo: struttura. Ogni taglio di un elenco buono va rifiutato.
  snprintf(t, sizeof t, "{\"schema\":1,\"a\":[1,-2.5e3,true,false,null,{\"b\":[]}],\"firmware\":{\"version\":\"1.8.19\",\"size\":7,\"sha256\":\"%s\",\"url\":\"https://x/y\",\"publisher\":\"a\\u00e8\"},\"recovery\":{\"version\":\"0.5.3\"}}", SHA);
  vero(leggi(t, d) && !strcmp(d.verRec, "0.5.3"), "elenco con tutti i tipi");
  size_t tutto = strlen(t);
  for (size_t i = 0; i < tutto; i++) vero(!aggLeggiUltima(t, i, d), "tagliato in un punto qualunque");
  for (size_t i = 0; i <= tutto; i++) {                 // nessun byte puo far leggere oltre o accettare il rotto: gira con i controlli di memoria
    char copia[900];
    memcpy(copia, t, tutto);
    copia[i < tutto ? i : tutto - 1] = '\x01';
    aggLeggiUltima(copia, tutto, d);
  }
  snprintf(t, sizeof t, "{\"schema\":1,\"firmware\":{\"version\":\"1.8.19\",\"size\":1,\"size\":2,\"sha256\":\"%s\",\"url\":\"https://x/y\"}}", SHA);
  vero(!leggi(t, d), "campo doppio");
  snprintf(t, sizeof t, "{\"schema\":1,\"firmware\":{\"version\":\"1.8.19\",\"size\":1,\"sha256\":\"%s\",\"url\":\"https://x/y\"}}xx", SHA);
  vero(!leggi(t, d), "testo dopo la fine");
  snprintf(t, sizeof t, "{\"schema\":1,\"firmware\":{\"version\":\"1.8.19\",\"size\":01,\"sha256\":\"%s\",\"url\":\"https://x/y\"}}", SHA);
  vero(!leggi(t, d), "zero in testa");
  snprintf(t, sizeof t, "{\"schema\":1,\"firmware\":{\"version\":\"1.8.19\",\"size\":1,\"sha256\":\"%s\",\"url\":\"https://x/y\",}}", SHA);
  vero(!leggi(t, d), "virgola finale");
  snprintf(t, sizeof t, "{\"schema\":1,\"firmware\":[1],\"altro\":2}");
  vero(!leggi(t, d), "firmware non oggetto");
  snprintf(t, sizeof t, "{\"schema\":1,\"a\":{\"b\":{\"c\":{\"d\":{\"e\":{\"f\":1}}}}},\"firmware\":{\"version\":\"1.8.19\",\"size\":1,\"sha256\":\"%s\",\"url\":\"https://x/y\"}}", SHA);
  vero(!leggi(t, d), "oggetti troppo annidati");
  vero(!leggi("{\"schema\":1,\"a\":tru", d), "parola tagliata");
  vero(!leggi("{", d), "solo graffa");
  vero(!leggi("{\"schema\"", d), "chiave senza valore");

  // orario: mercoledi 5 novembre, 03:30
  struct tm o = {};
  o.tm_hour = 3; o.tm_min = 30; o.tm_wday = 3; o.tm_mday = 5;
  vero(aggOrarioGiusto(o, 0, 1, 210), "ogni giorno");
  vero(!aggOrarioGiusto(o, 0, 1, 211), "minuto diverso");
  vero(aggOrarioGiusto(o, 1, 3, 210), "mercoledi = 3");
  vero(!aggOrarioGiusto(o, 1, 4, 210), "non giovedi");
  o.tm_wday = 0;
  vero(aggOrarioGiusto(o, 1, 7, 210), "domenica = 7");
  vero(aggOrarioGiusto(o, 2, 5, 210), "giorno 5 del mese");
  vero(!aggOrarioGiusto(o, 2, 6, 210), "non il 6");
  if (!falliti) puts("ok");
  return falliti ? 1 : 0;
}
CPP
c++ -std=gnu++17 -w -fsanitize=address,undefined -g -I"$R/VesevOS/src" -I"$R/recovery/VesevOS_Recovery_IDF" "$O/prova.cpp" "$R/VesevOS/src/packages/vos_aggiorna_dati.cpp" "$R/VesevOS/src/core/vos_pacchetto.cpp" -o "$O/prova" 2>&1 | head -30
"$O/prova"; rc=$?
rm -rf "$O"; exit $rc
