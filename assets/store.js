// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// Elenco dei pacchetti (app, driver, script) letto da index.json: icona, descrizione, autore, versione, licenza, SHA-256 e Download.
(function () {
  'use strict';
  var dati = null, filtro = 'tutti';

  function T(k) { return window.SITO.tr(k); }
  function el(tag, cls, testo) { var e = document.createElement(tag); if (cls) e.className = cls; if (testo !== undefined) e.textContent = testo; return e; }
  function copia(testo, bottone) {
    var fatto = function () { var v = bottone.textContent; bottone.textContent = T('s_copiato'); setTimeout(function () { bottone.textContent = T('s_copia'); }, 1500); return v; };
    if (navigator.clipboard && navigator.clipboard.writeText) navigator.clipboard.writeText(testo).then(fatto, function () { /* copia non consentita: resta il testo da selezionare */ });
  }

  function scheda(p) {
    var c = el('article', 'card');
    var riga = el('div', 'pk');
    var ico = el('div', 'ico'); var im = el('img'); im.src = p.icon; im.alt = ''; im.width = 28; im.height = 28; ico.appendChild(im);
    var cor = el('div', 'cor');
    var h = el('h3', '', p.name + ' ');
    h.appendChild(el('span', 'pill v', p.ext));
    cor.appendChild(h);
    cor.appendChild(el('p', '', (p.desc && (p.desc[window.SITO.lang] || p.desc.it)) || ''));
    var meta = el('div', 'meta');
    meta.appendChild(el('span', '', T('s_t_' + p.type)));
    meta.appendChild(el('span', '', T('s_autore') + ': ' + p.author));
    meta.appendChild(el('span', '', T('s_ver') + ': ' + p.version));
    meta.appendChild(el('span', '', T('s_lic') + ': ' + p.license));
    cor.appendChild(meta);
    var sha = el('div', 'sha');
    sha.appendChild(el('span', '', T('s_sha') + ':'));
    sha.appendChild(el('code', '', p.sha256));
    var b = el('button', 'btn gray sm', T('s_copia')); b.type = 'button';
    b.addEventListener('click', function () { copia(p.sha256, b); });
    sha.appendChild(b);
    cor.appendChild(sha);
    var az = el('div', 'az');
    var a = el('a', 'btn', T('s_scarica')); a.href = p.file; a.setAttribute('download', '');
    az.appendChild(a);
    cor.appendChild(az);
    riga.appendChild(ico); riga.appendChild(cor); c.appendChild(riga);
    return c;
  }

  function disegna() {
    var f = document.getElementById('filtri'), e = document.getElementById('elenco');
    f.textContent = ''; e.textContent = '';
    [['tutti', 's_tutti'], ['app', 's_app'], ['driver', 's_driver'], ['script', 's_script']].forEach(function (x) {
      var b = el('button', x[0] === filtro ? 'on' : '', T(x[1])); b.type = 'button';
      b.setAttribute('aria-pressed', x[0] === filtro ? 'true' : 'false');
      b.addEventListener('click', function () { filtro = x[0]; disegna(); });
      f.appendChild(b);
    });
    if (!dati) return;
    var lista = dati.items.filter(function (p) { return filtro === 'tutti' || p.type === filtro; });
    document.getElementById('msg').textContent = lista.length ? '' : T('s_vuoto');
    lista.forEach(function (p) { e.appendChild(scheda(p)); });
  }

  function carica() {
    fetch('index.json', { cache: 'no-cache' }).then(function (r) { if (!r.ok) throw new Error(r.status); return r.json(); })
      .then(function (j) { dati = j; disegna(); })
      .catch(function () { document.getElementById('msg').textContent = T('s_errore'); disegna(); });
  }
  document.addEventListener('sito-pronto', function () { disegna(); carica(); });
  document.addEventListener('sito-lang', disegna);
})();
