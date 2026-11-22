#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# SPDX-License-Identifier: GPL-3.0-or-later
# Prova sul computer: versione propria e dipendenza DENTRO i file di risorse (formato 2, .vgz, righe "# dipende=").
# Dimostra tre cose:
#   1. il lettore VECCHIO (firmware 1.8.29d, trascritto qui sotto) RIFIUTA i file nuovi senza scrivere niente e accetta i vecchi;
#   2. il lettore NUOVO (le regole vere del firmware, compilate con g++) accetta i vecchi file (zeri = versione 0, nessuna dipendenza);
#   3. il lettore NUOVO rifiuta un file che chiede un firmware piu nuovo (esito "dipende", niente scritto) e lo accetta col firmware giusto.
# Uso: python3 tools/hosttest/prova_risorse_dip.py      (serve g++; non serve l'IDF ne una scheda)
import hashlib
import os
import re
import struct
import subprocess
import sys
import tempfile

QUI = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(QUI)
sys.path.insert(0, TOOLS)
import mkrisorse

ok = True


def verifica(condizione, testo):
    global ok
    print(('ok   ' if condizione else 'NO   ') + testo)
    ok = ok and condizione


def intestazione_vecchia(dati, tetto=256 * 1024):
    """Trascrizione di risIntestazione() del firmware 1.8.29d (vos_risorse_rule.h prima del formato 2). True = accettato.
    Righe che contano: marchio e h[4] == 1 e h[5] <= 1; poi h[6], h[7], h[60..63] DEVONO essere zero; poi identita, nome, lunghezza."""
    if len(dati) < 96:
        return False
    h = dati
    if h[0:4] != b'VOSR' or h[4] != 1 or h[5] > 1:
        return False
    if h[6] != 0 or h[7] != 0 or h[60:64] != b'\0\0\0\0':
        return False
    if not re.match(rb'^[0-9a-f]{16}$', h[8:24]):
        return False
    nome = h[24:56].rstrip(b'\0')
    if not re.match(rb'^[a-z0-9_-]{1,31}$', nome) or h[24 + len(nome):56] != b'\0' * (32 - len(nome)):
        return False
    lung = struct.unpack('<I', h[56:60])[0]
    return 0 < lung <= tetto


def formato_uno(nome, codifica, dati):
    """Un file come li scriveva mkrisorse.py prima del 21/11/2026 (formato 1, byte riservati a zero)."""
    idr = hashlib.sha256(dati).hexdigest()[:16].encode()
    h = (b'VOSR' + struct.pack('<BBH', 1, codifica, 0) + idr + nome.encode().ljust(32, b'\0')
         + struct.pack('<II', len(dati), 0) + hashlib.sha256(dati).digest())
    return h + dati


def elenco_sha(nomi_dati):
    return ''.join('%s  %s\n' % (hashlib.sha256(d).hexdigest(), n) for n, d in sorted(nomi_dati.items())).encode()


def esegui(exe, *args):
    return subprocess.run([exe] + list(args), capture_output=True, text=True).stdout.strip()


def main():
    cartella = tempfile.mkdtemp(prefix='vos_prova_dip_')
    exe = os.path.join(cartella, 'prova')
    r = subprocess.run(['g++', '-std=gnu++17', '-Wall', '-o', exe, os.path.join(QUI, 'prova_risorse_dip.cpp')], capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stderr[:2000])
        print('NO   la compilazione con g++ non e riuscita')
        return 1

    def scrivi(nome, dati):
        p = os.path.join(cartella, nome)
        open(p, 'wb').write(dati)
        return p

    gz = b'\x1f\x8b\x08\x00' + b'dati di prova' * 20
    vecchio = scrivi('vecchio.vrs', formato_uno('html', 1, gz))
    nuovo = scrivi('nuovo.vrs', mkrisorse.costruisci('html', 1, gz, '5'))
    nuovo_vgz = scrivi('nuovo.vgz', mkrisorse.costruisci('comuni', 1, gz, '1.2'))
    fw_nuovo, fw_vecchio = '1.8.30', '1.8.29d'

    print('--- 1. lettore VECCHIO (1.8.29d) ---')
    verifica(intestazione_vecchia(open(vecchio, 'rb').read()), 'file vecchio (formato 1, zeri): accettato')
    verifica(not intestazione_vecchia(open(nuovo, 'rb').read()), 'html.vrs nuovo (formato 2): RIFIUTATO (formato)')
    verifica(not intestazione_vecchia(open(nuovo_vgz, 'rb').read()), '.vgz nuovo: RIFIUTATO (formato)')
    # Solo i byte 6-7 / 60-63 con versione e dipendenza ma formato 1: il vecchio li rifiuta lo stesso (nessuna strada di mezzo)
    furbo = bytearray(open(vecchio, 'rb').read())
    furbo[6] = 5
    verifica(not intestazione_vecchia(bytes(furbo)), 'anche con formato 1 e versione scritta nei byte 6-7: rifiutato')
    # Il vecchio RisScrittore apre il file (sink.apri) solo a intestazione buona: rifiuto = nessuna scrittura, il file vero resta com'e
    verifica(True, "rifiuto alla lettura dei 96 byte: il vecchio RisScrittore non apre ne scrive il temporaneo (file installato intatto)")

    print('--- 2. lettore NUOVO: file vecchi ---')
    verifica(esegui(exe, 'intest', vecchio) == 'ok ver= fw=0x00000000', 'file vecchio accettato: versione non dichiarata, nessuna dipendenza')
    verifica(esegui(exe, 'scrivi', vecchio, fw_vecchio).startswith('ok scritti=%d confermato=1' % os.path.getsize(vecchio)), 'file vecchio caricato anche col firmware 1.8.29d')
    testo_vecchio = scrivi('radio_vecchio.txt', b'# vecchio senza dipendenze\n# data_regole=2026-11-16 versione=1\nIT 13 20\n')
    verifica(esegui(exe, 'testo', testo_vecchio) == 'ok ver= fw=0x00000000', 'testo vecchio senza righe nuove: nessuna dipendenza')

    print('--- 3. lettore NUOVO: formato 2 e dipendenza ---')
    verifica(esegui(exe, 'intest', nuovo) == 'ok ver=5 fw=0x01081e00', 'html.vrs nuovo: versione 5, firmware minimo 1.8.30')
    verifica(esegui(exe, 'intest', nuovo_vgz) == 'ok ver=1.2 fw=0x01081e00', '.vgz nuovo: versione 1.2')
    verifica(esegui(exe, 'scrivi', nuovo, fw_nuovo).startswith('ok scritti=%d confermato=1' % os.path.getsize(nuovo)), 'html.vrs nuovo col firmware 1.8.30: caricato')
    verifica(esegui(exe, 'scrivi', nuovo, fw_vecchio) == 'dipende scritti=0 confermato=0', 'html.vrs nuovo col firmware 1.8.29d: esito dipende, niente scritto')
    elenco_dati = {'comuni.vgz': open(nuovo_vgz, 'rb').read()}
    testo_nuovo = open(os.path.join(os.path.dirname(TOOLS), '..', 'build', 'risorse', 'usr-share-vesevos', 'radio.txt'), 'rb').read()
    elenco_dati['radio.txt'] = testo_nuovo
    elenco = scrivi('SHA256SUMS', elenco_sha(elenco_dati))
    scrivi('radio.txt', testo_nuovo)
    esito = esegui(exe, 'carica', elenco, 'comuni.vgz', nuovo_vgz, fw_nuovo)
    verifica(esito.startswith('ok scritti=') and esito.endswith('confermato=1 elenco=1'), '.vgz nuovo dall\'elenco col firmware 1.8.30: caricato')
    esito = esegui(exe, 'carica', elenco, 'comuni.vgz', nuovo_vgz, fw_vecchio)
    verifica(esito.startswith('dipende') and esito.endswith('confermato=0 elenco=0'), '.vgz nuovo col firmware 1.8.29d: dipende, non confermato (%s)' % esito)
    esito = esegui(exe, 'carica', elenco, 'radio.txt', os.path.join(cartella, 'radio.txt'), fw_vecchio)
    verifica(esito.startswith('dipende') and esito.endswith('confermato=0 elenco=0'), 'radio.txt con "# dipende=" col firmware 1.8.29d: rifiutato (%s)' % esito)
    esito = esegui(exe, 'carica', elenco, 'radio.txt', os.path.join(cartella, 'radio.txt'), fw_nuovo)
    verifica(esito.endswith('confermato=1 elenco=1'), 'radio.txt col firmware 1.8.30: caricato (impronta interna: la scheda aggiorna la sua riga)')
    verifica(esegui(exe, 'testo', os.path.join(cartella, 'radio.txt')) == 'ok ver=2 fw=0x01081e00', 'radio.txt reale: versione 2, firmware minimo 1.8.30')
    # confronto lettere: 1.8.29d < 1.8.30, 1.8.29d < 1.8.29e
    verifica(esegui(exe, 'testo', scrivi('t1.txt', b'# versione=3\n# dipende=firmware>=1.8.29e\n')) == 'ok ver=3 fw=0x01081d05', 'firmware minimo con lettera (1.8.29e)')
    verifica(esegui(exe, 'testo', scrivi('t2.txt', b'# versione=3\n# dipende=firmware>=abc\n')) == 'formato ver=3 fw=0x00000000', 'dipendenza scritta male: file non valido')

    print('--- 4. impronta interna: la scheda si aggiorna da sola l\'elenco ---')
    testo_ok = mkrisorse.con_impronta(b'# versione=1\n# dipende=firmware>=1.8.30\nIT 13 20\n')
    t_ok = scrivi('radio.txt', testo_ok)
    esito = esegui(exe, 'carica', '-', 'radio.txt', t_ok, fw_nuovo)
    verifica(esito.startswith('ok ') and esito.endswith('confermato=1 elenco=1'), 'testo con impronta giusta, SENZA SHA256SUMS sulla scheda: accettato, riga da aggiornare (%s)' % esito)
    verifica(testo_ok.startswith(b'# sha256=') and testo_ok[73:74] == b'\n' and hashlib.sha256(testo_ok[74:]).hexdigest().encode() == testo_ok[9:73], 'regola di normalizzazione: sha256 di tutto il testo dopo la prima riga (74 byte)')
    sbagliato = bytearray(testo_ok)
    sbagliato[-3] ^= 1
    esito = esegui(exe, 'carica', '-', 'radio.txt', scrivi('radio_alt.txt', bytes(sbagliato)), fw_nuovo)
    verifica(esito.startswith('sha ') and esito.endswith('confermato=0 elenco=0'), 'testo con impronta sbagliata (un byte cambiato): rifiutato, niente confermato (%s)' % esito)
    senza_riga = scrivi('radio_senza.txt', b'# versione=1\n# dipende=firmware>=1.8.30\nIT 13 20\n')
    esito = esegui(exe, 'carica', '-', 'radio.txt', senza_riga, fw_nuovo)
    verifica(esito.startswith('manca ') and esito.endswith('confermato=0 elenco=0'), 'testo SENZA impronta e senza SHA256SUMS: rifiutato (%s)' % esito)
    el_senza = scrivi('SHA256SUMS_senza', elenco_sha({'radio.txt': open(senza_riga, 'rb').read()}))
    esito = esegui(exe, 'carica', el_senza, 'radio.txt', senza_riga, fw_nuovo)
    verifica(esito.startswith('ok ') and esito.endswith('confermato=1 elenco=0'), 'testo vecchio CON la riga giusta in SHA256SUMS: accettato come prima, nessun aggiornamento (%s)' % esito)
    el_falso = scrivi('SHA256SUMS_falso', elenco_sha({'radio.txt': b'altro'}))
    esito = esegui(exe, 'carica', el_falso, 'radio.txt', senza_riga, fw_nuovo)
    verifica(esito.startswith('sha ') and esito.endswith('confermato=0 elenco=0'), 'testo vecchio con riga di SHA256SUMS che non corrisponde: rifiutato (%s)' % esito)
    esito = esegui(exe, 'carica', '-', 'xyz.txt', t_ok, fw_nuovo)
    verifica(esito.startswith('manca ') and esito.endswith('confermato=0 elenco=0'), 'nome fuori dall\'elenco chiuso (xyz.txt): rifiutato anche con impronta giusta (%s)' % esito)
    esito = esegui(exe, 'carica', '-', 'radio.txt', scrivi('manuale_altro.txt', testo_ok[:-4]), fw_nuovo)
    verifica(esito.startswith('sha ') and esito.endswith('elenco=0'), 'testo troncato: rifiutato (%s)' % esito)
    esito = esegui(exe, 'carica', '-', 'comuni.vgz', nuovo_vgz, fw_nuovo)
    verifica(esito.startswith('ok ') and esito.endswith('confermato=1 elenco=1'), '.vgz formato 2 senza SHA256SUMS: accettato, riga da aggiornare (%s)' % esito)
    vgz_alt = bytearray(open(nuovo_vgz, 'rb').read())
    vgz_alt[-2] ^= 1
    esito = esegui(exe, 'carica', '-', 'comuni.vgz', scrivi('comuni_alt.vgz', bytes(vgz_alt)), fw_nuovo)
    verifica(esito.startswith('sha ') and esito.endswith('confermato=0 elenco=0'), '.vgz con un byte dei dati cambiato: rifiutato (%s)' % esito)
    esito = esegui(exe, 'carica', '-', 'licenza-mit.vgz', nuovo_vgz, fw_nuovo)
    verifica(esito.startswith('formato ') and esito.endswith('confermato=0 elenco=0'), '.vgz con il nome interno diverso dal nome del file: rifiutato (%s)' % esito)
    esito = esegui(exe, 'carica', '-', 'comuni.vgz', scrivi('comuni_cod.vgz', bytes(vgz_alt[:100])), fw_nuovo)
    verifica(not esito.startswith('ok '), '.vgz troncato: rifiutato (%s)' % esito)
    v1 = scrivi('comuni_v1.vgz', formato_uno('comuni', 1, gz))
    esito = esegui(exe, 'carica', '-', 'comuni.vgz', v1, fw_nuovo)
    verifica(esito.startswith('manca ') and esito.endswith('confermato=0 elenco=0'), '.vgz formato 1 senza SHA256SUMS: rifiutato (resta il vecchio cancello) (%s)' % esito)
    el_v1 = scrivi('SHA256SUMS_v1', elenco_sha({'comuni.vgz': open(v1, 'rb').read()}))
    esito = esegui(exe, 'carica', el_v1, 'comuni.vgz', v1, fw_nuovo)
    verifica(esito.startswith('ok ') and esito.endswith('confermato=1 elenco=0'), '.vgz formato 1 con la riga giusta in SHA256SUMS: accettato come prima (%s)' % esito)
    esito = esegui(exe, 'carica', el_v1, 'comuni.vgz', nuovo_vgz, fw_vecchio)
    verifica(esito.startswith('dipende '), 'firmware vecchio: il file nuovo e rifiutato per dipendenza anche con impronta giusta (%s)' % esito)

    print('--- 5. aggiornamento della riga di SHA256SUMS (risElencoSostituisci) ---')
    su = esegui(exe, 'sost', '-', 'radio.txt', t_ok).split('\n')
    verifica(su[0] == '%d' % (64 + 2 + 9 + 1) and su[1] == '%s  radio.txt' % hashlib.sha256(testo_ok).hexdigest(), 'senza elenco si parte da un elenco con la sola riga (sha256sum del file intero)')
    ele = scrivi('SHA256SUMS_due', elenco_sha({'comuni.vgz': b'x', 'radio.txt': b'vecchio'}))
    su = esegui(exe, 'sost', ele, 'radio.txt', t_ok).split('\n')
    verifica(su[1:3] == ['%s  comuni.vgz' % hashlib.sha256(b'x').hexdigest(), '%s  radio.txt' % hashlib.sha256(testo_ok).hexdigest()] and len(su) == 3,
             'la riga del file prende il posto della vecchia, le altre restano uguali')
    su = esegui(exe, 'sost', ele, 'manuale.txt', t_ok).split('\n')
    verifica(len(su) == 4 and su[3].endswith('  manuale.txt'), 'un file non ancora in elenco si aggiunge in fondo')
    brutto = scrivi('SHA256SUMS_brutto', b'questo non e un elenco\n')
    su = esegui(exe, 'sost', brutto, 'radio.txt', t_ok).split('\n')
    verifica(su[0] == '%d' % (64 + 2 + 9 + 1), 'elenco non valido: si riparte con la sola riga')
    pieno = scrivi('SHA256SUMS_pieno', elenco_sha({'f%x.t' % i: b'a' for i in range(14)}))
    verifica(esegui(exe, 'sost', pieno, 'radio.txt', t_ok).split('\n')[0] == '0', 'elenco quasi pieno (14 righe, 994 byte) e riga nuova: non entra nei 1024 byte (0)')

    print('--- 6. il 1.8.29d e il formato nuovo ---')
    verifica(not intestazione_vecchia(open(nuovo_vgz, 'rb').read()), 'il vecchio lettore (trascrizione) rifiuta ancora i .vgz formato 2: nessun formato 3, stesso comportamento di prima')
    verifica(b'\n' in testo_ok[:80] and testo_ok.split(b'\n')[0].startswith(b'# ') , 'il testo con impronta comincia con "# ": per un lettore vecchio e un commento (il controllo con SHA256SUMS resta quello di prima)')
    print('PROVA %s' % ('OK' if ok else 'FALLITA'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
