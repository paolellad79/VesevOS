# SPDX-License-Identifier: GPL-3.0-or-later
# Versioni PROPRIE dei file di risorse (pagina, radio, manuale): un solo posto, tools/data/versioni_risorse.txt.
# Il firmware non entra: cambiare VOS_VERSION non cambia nessun file di risorse.
import os

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
