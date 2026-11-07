// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
//
// Terminale seriale nel browser (Web Serial). Il log resta nel browser:
// non viene salvato e non viene inviato a nessuno.

(function () {
  'use strict';

  var BAUD = 115200;
  var MAX_LOG = 200000;      // caratteri: se il log e piu lungo, si tagliano le righe vecchie

  var porta = null;
  var lettore = null;
  var scrittore = null;
  var ciclo = null;          // promessa del ciclo di lettura
  var log = '';

  function $(id) { return document.getElementById(id); }

  var el = {
    collega: $('collega'),
    scollega: $('scollega'),
    stato: $('stato'),
    nobrowser: $('nobrowser'),
    uscita: $('uscita'),
    invio: $('invio'),
    pulisci: $('pulisci'),
    copia: $('copia'),
    messaggio: $('messaggio')
  };

  function stato(testo) { el.stato.textContent = testo; }
  function messaggio(testo) { el.messaggio.textContent = testo; }

  function aggiungi(testo) {
    log += testo;
    if (log.length > MAX_LOG) log = log.slice(log.length - MAX_LOG);
    el.uscita.textContent = log;
    el.uscita.scrollTop = el.uscita.scrollHeight;
  }

  function pulsanti(collegata) {
    el.collega.disabled = collegata;
    el.scollega.disabled = !collegata;
    el.invio.disabled = !collegata;
  }

  // Legge i dati dalla scheda finche la porta e aperta.
  function leggi() {
    var decodifica = new TextDecoder();
    return (async function () {
      try {
        while (true) {
          var r = await lettore.read();
          if (r.done) break;
          aggiungi(decodifica.decode(r.value, { stream: true }));
        }
      } catch (e) {
        // Se la porta e ancora aperta, la lettura si e fermata da sola (scheda staccata?).
        if (porta) stato('Lettura interrotta. La scheda potrebbe essere stata staccata: premi Scollega.');
      } finally {
        try { lettore.releaseLock(); } catch (e) { /* gia rilasciato */ }
      }
    })();
  }

  async function collega() {
    if (porta) return;
    try {
      porta = await navigator.serial.requestPort();
      await porta.open({ baudRate: BAUD });
    } catch (e) {
      porta = null;
      stato('Collegamento annullato o non riuscito.');
      return;
    }
    lettore = porta.readable.getReader();
    scrittore = porta.writable.getWriter();
    ciclo = leggi();
    stato('Collegata a ' + BAUD + ' baud.');
    pulsanti(true);
    el.invio.focus();
  }

  async function scollega() {
    if (!porta) return;
    var p = porta;
    porta = null;                 // cosi la lettura non segnala errori durante la chiusura
    pulsanti(false);
    try { await lettore.cancel(); } catch (e) { /* lettura gia ferma */ }
    try { await ciclo; } catch (e) { /* gia gestito nel ciclo */ }
    try { scrittore.releaseLock(); } catch (e) { /* scrittura gia chiusa */ }
    try { await p.close(); } catch (e) { /* porta gia chiusa o scomparsa */ }
    lettore = null;
    scrittore = null;
    ciclo = null;
    stato('Scheda scollegata.');
  }

  async function invia(riga) {
    if (!scrittore) return;
    await scrittore.write(new TextEncoder().encode(riga + '\r\n'));
  }

  function pulisci() {
    log = '';
    el.uscita.textContent = '';
    messaggio('Log pulito.');
  }

  async function copia() {
    if (!log) {
      messaggio('Il log e vuoto.');
      return;
    }
    try {
      await navigator.clipboard.writeText(log);
      messaggio('Log copiato negli appunti.');
    } catch (e) {
      messaggio('Copia non riuscita. Seleziona il testo nell\'area e copialo a mano.');
    }
  }

  el.collega.addEventListener('click', collega);
  el.scollega.addEventListener('click', scollega);
  el.pulisci.addEventListener('click', pulisci);
  el.copia.addEventListener('click', copia);

  el.invio.addEventListener('keydown', async function (e) {
    if (e.key !== 'Enter' || e.isComposing) return;
    e.preventDefault();
    var testo = el.invio.value;
    el.invio.value = '';
    try {
      await invia(testo);
    } catch (err) {
      messaggio('Invio non riuscito. Controlla il collegamento.');
    }
  });

  if (!navigator.serial) {
    el.nobrowser.hidden = false;
    el.collega.disabled = true;
    stato('Browser non supportato');
  } else {
    pulsanti(false);
  }
})();
