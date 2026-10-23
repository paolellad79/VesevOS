// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
//
// Scrittura sulla scheda con esptool-js (Apache-2.0, copia locale in vendor/).
// Qui NON ci sono controlli sui file: li fa controlli.js prima di arrivare qui.

import { t, tf } from './lang.js';

const LIBRERIA = './vendor/esptool-js-0.5.7.bundle.js';
const BAUD_SCRITTURA = 460800;
const BAUD_ROM = 115200;
const VID_SCHEDA = 0x303a;      // costruttore del chip: serve solo a filtrare le porte
const PID_USB_NATIVA = 0x1001;

// esptool-js vuole i dati come "stringa binaria": un carattere per ogni byte.
function inStringaBinaria(byte) {
  let s = '';
  const pezzo = 0x8000;
  for (let i = 0; i < byte.length; i += pezzo) {
    s += String.fromCharCode.apply(null, byte.subarray(i, i + pezzo));
  }
  return s;
}

const hex = (n) => n.toString(16);

// parti: [{ nome, indirizzo, byte }] in ordine di indirizzo.
// ui: { registra(testo, errore), stato(testo, percento) }
export async function scriviParti(parti, ui) {
  const { ESPLoader, Transport } = await import(LIBRERIA);

  ui.registra(t('f_inizio'));
  let porta;
  try {
    porta = await navigator.serial.requestPort({ filters: [{ usbVendorId: VID_SCHEDA }] });
  } catch (e) {
    ui.registra(t('f_porta_annullata'));
    ui.stato(t('f_porta_annullata'), 0);
    return;
  }

  const trasporto = new Transport(porta, false);
  // Il testo di esptool-js resta nella console del browser: puo nominare marchi.
  const terminale = {
    clean() {},
    writeLine(d) { console.debug(d); },
    write(d) { console.debug(d); }
  };
  try {
    ui.registra(t('f_connessione'));
    ui.stato(t('f_connessione'), 0);
    const caricatore = new ESPLoader({
      transport: trasporto, baudrate: BAUD_SCRITTURA, romBaudrate: BAUD_ROM, terminal: terminale
    });
    await caricatore.main();
    if (!caricatore.chip || caricatore.chip.CHIP_NAME !== 'ESP32-S3') {
      ui.registra(t('f_chip_ko'), true);
      ui.stato(t('f_chip_ko'), 0);
      return;
    }
    ui.registra(t('f_connessa'));

    const totale = parti.reduce((somma, p) => somma + p.byte.length, 0);
    const finiti = new Array(parti.length).fill(0);
    const fileArray = parti.map((p) => ({ data: inStringaBinaria(p.byte), address: p.indirizzo }));
    parti.forEach((p) => ui.registra(tf('f_scrivo', p.nome, p.byte.length, hex(p.indirizzo))));

    await caricatore.writeFlash({
      fileArray,
      flashSize: 'keep', flashMode: 'keep', flashFreq: 'keep',
      eraseAll: false,
      compress: true,
      reportProgress: (indice, scritti, tot) => {
        // scritti/tot sono sul file compresso: li riportiamo alla dimensione vera
        finiti[indice] = tot > 0 ? Math.round(parti[indice].byte.length * scritti / tot) : 0;
        const fatto = finiti.reduce((a, b) => a + b, 0);
        const perc = Math.min(100, Math.floor(100 * fatto / totale));
        ui.stato(tf('f_scrivo', parti[indice].nome, parti[indice].byte.length, hex(parti[indice].indirizzo)) + ' ' + perc + '%', perc);
        if (scritti >= tot && !parti[indice].fine) {
          parti[indice].fine = true;
          ui.registra(tf('f_parte_fine', parti[indice].nome));
        }
      }
    });

    ui.registra(t('f_riavvio'));
    await caricatore.after('hard_reset', trasporto.getPid() === PID_USB_NATIVA);
    ui.stato(t('f_fine'), 100);
    ui.registra(t('f_fine'));
  } finally {
    try { await trasporto.disconnect(); } catch (e) { /* la porta potrebbe essere gia sparita col riavvio */ }
  }
}
