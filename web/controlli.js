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
  firmware:   { indirizzo: 0x110000, max: 2424832 }
};

const MARCHIO_FW = '{FW:VesevOS}';
const MARCHIO_REC = 'VesevOS Recovery';

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
  } else {
    r.c_marchio = true;                              // non applicabile
  }

  // (c) dimensione nello spazio previsto (e uguale a quella dell'elenco)
  r.c_dim = byte.length > 0 && byte.length <= regola.max && byte.length === parte.dimensione &&
            (!regola.esatta || byte.length === regola.esatta);
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
