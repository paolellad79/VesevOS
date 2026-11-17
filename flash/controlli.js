// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
//
// Controlli sui file prima di abilitare la scrittura.
// I limiti stanno QUI nel codice, non nell'elenco firmware.json:
// l'elenco dice solo quale file e quale SHA-256, non cosa e permesso.

// Per ogni tipo di file: indirizzo di scrittura ammesso e dimensione massima.
export const REGOLE = {
  bootloader: { indirizzo: 0x0,      max: 0x8000 },     // fino alla tabella
  tabella:    { indirizzo: 0x8000,   max: 0x1000 },
  nvs:        { indirizzo: 0x9000,   max: 0x5000, esatta: 0x5000 },
  otadata:    { indirizzo: 0xe000,   max: 0x2000, esatta: 0x2000 },
  recovery:   { indirizzo: 0x10000,  max: 1048576 },    // 1 MiB
  firmware:   { indirizzo: 0x110000, max: 2424832 },
  // immagine corta del file system (LittleFS) con i file di /usr/share/vesevos: SOLO installazione nuova, mai un aggiornamento.
  // Si scrive all'inizio della partizione dei file (0x360000, grande 0xA0000); la scheda la estende da sola al primo avvio.
  risorse:    { indirizzo: 0x360000, max: 0xA0000, blocco: 0x1000, minimo: 8 * 0x1000 }
};

const MARCHIO_FW = '{FW:VesevOS}';
const MARCHIO_REC = 'VesevOS Recovery';
const MAGIC_LFS = 'littlefs';          // nome scritto da LittleFS nel superblocco (dal byte 8 di ciascuno dei due blocchi iniziali)
const VERSIONE_LFS = 0x00020001;       // formato su disco 2.1 (LFS_DISK_VERSION di LittleFS 2.11)

// Cerca un testo ASCII dentro i byte, senza creare stringhe enormi.
export function contiene(byte, testo) {
  const n = testo.length;
  const primo = testo.charCodeAt(0);
  for (let i = 0, fine = byte.length - n; i <= fine; i++) {
    if (byte[i] !== primo) continue;
    let j = 1;
    while (j < n && byte[i + j] === testo.charCodeAt(j)) j++;
    if (j === n) return true;
  }
  return false;
}

export async function sha256Hex(byte) {
  const h = await crypto.subtle.digest('SHA-256', byte);
  return Array.from(new Uint8Array(h), (b) => b.toString(16).padStart(2, '0')).join('');
}

// Controlla che l'immagine del file system abbia un superblocco LittleFS valido: nome, versione 2.x, blocco da 4096 byte
// e numero di blocchi uguale alla lunghezza del file (l'immagine e corta: la scheda la estende al primo avvio).
function superblocco(byte, blocco) {
  for (let b = 0; b < 2; b++) {
    const base = b * blocco;
    if (byte.length < base + 32) continue;
    let nome = '';
    for (let i = 0; i < MAGIC_LFS.length; i++) nome += String.fromCharCode(byte[base + 8 + i]);
    if (nome !== MAGIC_LFS) continue;
    const v = new DataView(byte.buffer, byte.byteOffset + base, 32);
    const versione = v.getUint32(20, true), dimBlocco = v.getUint32(24, true), nBlocchi = v.getUint32(28, true);
    if ((versione >>> 16) === (VERSIONE_LFS >>> 16) && dimBlocco === blocco && nBlocchi * blocco === byte.length) return true;
  }
  return false;
}

// Controlla una parte. Restituisce { c_byte, c_marchio, c_dim, c_sha, errore }.
// Ogni c_* e true / false; errore e { chiave, args } del primo controllo fallito.
export async function controllaParte(id, parte, byte) {
  const r = { c_byte: false, c_marchio: false, c_dim: false, c_sha: false, errore: null };
  const regola = REGOLE[parte.tipo];
  const guasto = (chiave, ...args) => { if (!r.errore) r.errore = { chiave, args: [id, ...args] }; };

  if (!regola || parte.indirizzo !== regola.indirizzo) { guasto('err_indirizzo'); return r; }

  // (a) primo byte 0xE9 per i programmi; per i dati un controllo di contenuto
  const eProgramma = ['bootloader', 'recovery', 'firmware'].includes(parte.tipo);
  if (eProgramma) {
    r.c_byte = byte.length > 0 && byte[0] === 0xE9;
    if (!r.c_byte) guasto('err_byte');
  } else if (parte.tipo === 'tabella') {
    r.c_byte = byte.length >= 2 && byte[0] === 0xAA && byte[1] === 0x50;
    if (!r.c_byte) guasto('err_contenuto');
  } else if (parte.tipo === 'risorse') {
    r.c_byte = superblocco(byte, regola.blocco);
    if (!r.c_byte) guasto('err_contenuto');
  } else if (parte.tipo === 'nvs') {
    r.c_byte = byte.every((b) => b === 0xFF);       // memoria vuota: nessun dato personale
    if (!r.c_byte) guasto('err_contenuto');
  } else {
    r.c_byte = byte.length === 0x2000;               // otadata
    if (!r.c_byte) guasto('err_contenuto');
  }

  // (b) marchio: il firmware deve averlo, il recovery no
  if (parte.tipo === 'firmware') {
    r.c_marchio = contiene(byte, MARCHIO_FW);
    if (!r.c_marchio) guasto('err_marchio');
  } else if (parte.tipo === 'recovery') {
    r.c_marchio = contiene(byte, MARCHIO_REC);
    if (!r.c_marchio) guasto('err_marchio');
    else if (contiene(byte, MARCHIO_FW)) { r.c_marchio = false; guasto('err_marchio_extra'); }
  } else if (parte.tipo === 'risorse') {
    // dentro devono esserci l'elenco SHA256SUMS, la pagina html.vrs e il marchio dei file di risorse (VOSR)
    r.c_marchio = contiene(byte, 'SHA256SUMS') && contiene(byte, 'html.vrs') && contiene(byte, 'VOSR');
    if (!r.c_marchio) guasto('err_marchio');
  } else {
    r.c_marchio = true;                              // non applicabile
  }

  // (c) dimensione nello spazio previsto (e uguale a quella dell'elenco)
  r.c_dim = byte.length > 0 && byte.length <= regola.max && byte.length === parte.dimensione &&
            (!regola.esatta || byte.length === regola.esatta) &&
            (!regola.blocco || (byte.length % regola.blocco === 0 && byte.length >= regola.minimo));
  if (!r.c_dim) guasto('err_dim', byte.length, regola.max);

  // (d) SHA-256 uguale a quello dell'elenco
  const calcolato = await sha256Hex(byte);
  r.c_sha = calcolato === String(parte.sha256).toLowerCase();
  if (!r.c_sha) guasto('err_sha');
  return r;
}

export function tuttoOk(r) {
  return !!r && r.c_byte && r.c_marchio && r.c_dim && r.c_sha && !r.errore;
}
