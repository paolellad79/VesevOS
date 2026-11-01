// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// Galleria della pagina iniziale: gli screenshot cambiano con la lingua (italiano o inglese).
(function () {
  'use strict';
  var FOTO = [['screenshot-home', 'g1'], ['screenshot-auto', 'g2'], ['screenshot-avvio', 'g3'], ['screenshot-telefono', 'g4'], ['screenshot-pin', 'g5']];
  var SOLO_IT = { 'screenshot-pin': true };      // senza versione inglese: la stessa immagine per tutte le lingue

  function disegna() {
    var g = document.getElementById('galleria');
    g.textContent = '';
    var en = window.SITO.lang !== 'it';
    FOTO.forEach(function (f) {
      var fig = document.createElement('figure');
      var img = document.createElement('img');
      img.src = 'assets/' + f[0] + (en && !SOLO_IT[f[0]] ? '.en' : '') + '.png';
      img.alt = window.SITO.tr(f[1]);
      img.loading = 'lazy';
      var cap = document.createElement('figcaption');
      cap.textContent = window.SITO.tr(f[1]);
      fig.appendChild(img); fig.appendChild(cap); g.appendChild(fig);
    });
  }
  document.addEventListener('sito-pronto', disegna);
  document.addEventListener('sito-lang', disegna);
})();
