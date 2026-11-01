// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// Tabelle dei componenti e delle licenze, lette da componenti.json (ricavato dalla distinta del software del progetto).
(function () {
  'use strict';
  var dati = null;
  function T(k) { return window.SITO.tr(k); }
  function riempi(id, righe) {
    var t = document.getElementById(id);
    t.textContent = '';
    var cap = t.createTHead().insertRow();
    ['l_nome', 'l_ver', 'l_licenza', 'l_dove'].forEach(function (k) { var th = document.createElement('th'); th.scope = 'col'; th.textContent = T(k); cap.appendChild(th); });
    var corpo = t.createTBody();
    righe.forEach(function (r) {
      var tr = corpo.insertRow();
      [r.nome, r.versione || '', r.licenza, r.origine || ''].forEach(function (v) { tr.insertCell().textContent = v; });
    });
  }
  function disegna() { if (!dati) return; riempi('tab-comp', dati.firmware); riempi('tab-sito', dati.sito); }
  function carica() {
    fetch('componenti.json', { cache: 'no-cache' }).then(function (r) { if (!r.ok) throw new Error(r.status); return r.json(); })
      .then(function (j) { dati = j; disegna(); })
      .catch(function () { document.getElementById('msg').textContent = T('l_errore'); });
  }
  document.addEventListener('sito-pronto', carica);
  document.addEventListener('sito-lang', disegna);
})();
