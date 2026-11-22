# SPDX-License-Identifier: GPL-3.0-or-later
# Versioni PROPRIE dei file di risorse (pagina, radio, manuale): un solo posto, tools/data/versioni_risorse.txt.
# Il firmware non entra: cambiare VOS_VERSION non cambia nessun file di risorse.
import os
import re

PERCORSO = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'data', 'versioni_risorse.txt')


def versione_risorsa(nome):
    for riga in open(PERCORSO, encoding='ascii'):
        riga = riga.strip()
        if riga.startswith('#') or '=' not in riga:
            continue
        chiave, valore = riga.split('=', 1)
        if chiave.strip() == nome:
            valore = valore.strip()
            # regola del 16/11/2026: solo cifre (intero), da 1; 0 e riservato a 'non dichiarata'
            if not valore.isdigit() or valore != str(int(valore)) or int(valore) < 1:
                raise SystemExit('ERRORE: versione "%s" = "%s": deve essere un intero da 1 (solo cifre)' % (nome, valore))
            return valore
    raise SystemExit('ERRORE: versione "%s" non trovata in %s' % (nome, PERCORSO))


def firmware_minimo():
    """Firmware piu vecchio che sa leggere i file di risorse nuovi (formato 2, .vgz): una sola riga della tabella."""
    for riga in open(PERCORSO, encoding='ascii'):
        riga = riga.strip()
        if riga.startswith('firmware_minimo='):
            valore = riga.split('=', 1)[1].strip()
            impacchetta_firmware(valore)          # controlla la forma
            return valore
    raise SystemExit('ERRORE: "firmware_minimo" non trovato in %s' % PERCORSO)


def impacchetta_firmware(testo):
    """'1.8.29d' -> 4 byte: maggiore, minore, patch, lettera (0 = nessuna, 1 = a...). Uguale a risFwImpacchetta() del firmware."""
    m = re.match(r'^(\d{1,3})\.(\d{1,3})\.(\d{1,3})([a-z]?)$', testo)
    if not m or any(int(m.group(i)) > 255 for i in (1, 2, 3)):
        raise SystemExit('ERRORE: versione del firmware "%s" non valida (esempio: 1.8.29d)' % testo)
    lettera = ord(m.group(4)) - 96 if m.group(4) else 0
    return bytes([int(m.group(1)), int(m.group(2)), int(m.group(3)), lettera])


def versione_due_byte(testo):
    """'5' -> (5, 0); '1.2' -> (1, 2): maggiore e minore della versione della risorsa."""
    m = re.match(r'^(\d{1,3})(?:\.(\d{1,3}))?$', testo)
    if not m or int(m.group(1)) > 255 or int(m.group(2) or 0) > 255:
        raise SystemExit('ERRORE: versione di risorsa "%s" non valida' % testo)
    return int(m.group(1)), int(m.group(2) or 0)


def righe_testo(nome):
    """Le due righe in cima ai file di testo (manuale.txt, radio.txt)."""
    return ['# versione=%s' % versione_risorsa(nome), '# dipende=firmware>=%s' % firmware_minimo()]
