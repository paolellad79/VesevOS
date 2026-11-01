// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// Parte comune del sito: barra in alto, tema giorno/notte/automatico e accessibilita (stessi nomi e stessa memoria della pagina della scheda),
// lingua scelta (chiave "vl"). Nessun dato esce dal browser.
(function () {
  'use strict';
  var SPRITE = '<svg width="0" height="0" aria-hidden="true" focusable="false"><defs><linearGradient id="vg" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#3fa7ff"/><stop offset="1" stop-color="#37e0b0"/></linearGradient><symbol id="i-chip" viewBox="-8 -8 216 216"><g stroke="url(#vg)" stroke-width="6" stroke-linecap="round" fill="none"><path d="M40 -6V22M80 -6V22M120 -6V22M40 178V206M80 178V206M120 178V206M-6 40H22M-6 80H22M-6 120H22M178 40H206M178 80H206M178 120H206"/><rect x="20" y="20" width="160" height="160" rx="22" fill="#0b1220"/><circle cx="100" cy="100" r="44"/><path d="M100 56V30M100 170V144M56 100H30M170 100H144"/></g><circle cx="100" cy="100" r="14" fill="url(#vg)"/></symbol><symbol id="i-chipn" viewBox="0 0 24 24"><rect x="6" y="6" width="12" height="12" rx="2"/><rect x="9.5" y="9.5" width="5" height="5" rx="1"/><path d="M9 3v3M15 3v3M9 18v3M15 18v3M3 9h3M3 15h3M18 9h3M18 15h3"/></symbol><symbol id="i-sun" viewBox="0 0 24 24"><circle cx="12" cy="12" r="4"/><path d="M12 2.5v2.2M12 19.3v2.2M2.5 12h2.2M19.3 12h2.2M5.3 5.3l1.6 1.6M17.1 17.1l1.6 1.6M5.3 18.7l1.6-1.6M17.1 6.9l1.6-1.6"/></symbol><symbol id="i-moon" viewBox="0 0 24 24"><path d="M20 14.5A8.5 8.5 0 1 1 9.5 4a6.8 6.8 0 0 0 10.5 10.5z"/></symbol><symbol id="i-auto" viewBox="0 0 24 24"><circle cx="12" cy="12" r="8.5"/><path d="M12 3.5v17a8.5 8.5 0 0 0 0-17z" fill="currentColor"/></symbol><symbol id="i-prog" viewBox="0 0 24 24"><path d="M9 7.5 4.5 12 9 16.5"/><path d="M15 7.5l4.5 4.5-4.5 4.5"/><path d="M13.2 5.5l-2.4 13"/></symbol><symbol id="i-dl" viewBox="0 0 24 24"><path d="M12 4v11"/><path d="M7.5 10.5 12 15l4.5-4.5"/><path d="M4.5 15v3.5a1.5 1.5 0 0 0 1.5 1.5h12a1.5 1.5 0 0 0 1.5-1.5V15"/></symbol><symbol id="i-fvsc" viewBox="0 0 24 24"><path d="M6.5 3h7.5l4.5 4.5v12a1.5 1.5 0 0 1-1.5 1.5h-10.5a1.5 1.5 0 0 1-1.5-1.5v-15a1.5 1.5 0 0 1 1.5-1.5z"/><path d="M14 3v4.5h4.5"/><path d="M7.8 19l1.8-4.6 1.5 2 1.8-3.8h1.5l2.5 6.4z"/><path d="M12.3 10.6c.6-.7-.5-1.1 0-1.8"/></symbol><symbol id="i-fvap" viewBox="0 0 24 24"><rect x="3.5" y="3.5" width="17" height="17" rx="4.5"/><path d="M6.8 18l2.2-4.8 1.7 2.4 2.1-4h1.7l2.8 6.4z"/><path d="M14.2 9.2c-.7-.8.6-1.3 0-2.3"/></symbol><symbol id="i-fcode" viewBox="0 0 24 24"><path d="M6.5 3h7.5l4.5 4.5v12a1.5 1.5 0 0 1-1.5 1.5h-10.5a1.5 1.5 0 0 1-1.5-1.5v-15a1.5 1.5 0 0 1 1.5-1.5z"/><path d="M14 3v4.5h4.5"/><path d="M10 12.5 7.8 15l2.2 2.5M14 12.5l2.2 2.5-2.2 2.5"/></symbol><symbol id="i-a11y" viewBox="0 0 24 24"><circle cx="12" cy="4.8" r="1.8"/><path d="M5 8.5c2.3.7 4.6 1 7 1s4.7-.3 7-1"/><path d="M12 9.5v4.5l-3 6.5M12 14l3 6.5"/></symbol><symbol id="i-gear" viewBox="0 0 24 24"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.7 1.7 0 0 0 .3 1.8l.1.1a2 2 0 1 1-2.8 2.8l-.1-.1a1.7 1.7 0 0 0-1.8-.3 1.7 1.7 0 0 0-1 1.5V21a2 2 0 1 1-4 0v-.1a1.7 1.7 0 0 0-1.1-1.5 1.7 1.7 0 0 0-1.8.3l-.1.1a2 2 0 1 1-2.8-2.8l.1-.1a1.7 1.7 0 0 0 .3-1.8 1.7 1.7 0 0 0-1.5-1H3a2 2 0 1 1 0-4h.1a1.7 1.7 0 0 0 1.5-1.1 1.7 1.7 0 0 0-.3-1.8l-.1-.1a2 2 0 1 1 2.8-2.8l.1.1a1.7 1.7 0 0 0 1.8.3H9a1.7 1.7 0 0 0 1-1.5V3a2 2 0 1 1 4 0v.1a1.7 1.7 0 0 0 1 1.5 1.7 1.7 0 0 0 1.8-.3l.1-.1a2 2 0 1 1 2.8 2.8l-.1.1a1.7 1.7 0 0 0-.3 1.8V9a1.7 1.7 0 0 0 1.5 1H21a2 2 0 1 1 0 4h-.1a1.7 1.7 0 0 0-1.5 1z"/></symbol><symbol id="i-term" viewBox="0 0 24 24"><rect x="3" y="4.5" width="18" height="15" rx="2.5"/><path d="M7 10l3 2.2L7 14.4M12.5 15h4.5"/></symbol><symbol id="i-disk" viewBox="0 0 24 24"><ellipse cx="12" cy="6" rx="7.5" ry="2.5"/><path d="M4.5 6v12c0 1.4 3.4 2.5 7.5 2.5s7.5-1.1 7.5-2.5V6"/><path d="M4.5 12c0 1.4 3.4 2.5 7.5 2.5s7.5-1.1 7.5-2.5"/></symbol><symbol id="i-folder" viewBox="0 0 24 24"><path d="M3 7a1.5 1.5 0 0 1 1.5-1.5h4.3l2 2h8.7A1.5 1.5 0 0 1 21 9v9.5a1.5 1.5 0 0 1-1.5 1.5h-15A1.5 1.5 0 0 1 3 18.5z"/></symbol><symbol id="i-info" viewBox="0 0 24 24"><circle cx="12" cy="12" r="8.5"/><path d="M12 11v5.5"/><path d="M12 7.8h.01"/></symbol><symbol id="i-shield" viewBox="0 0 24 24"><path d="M12 3 4.5 6v5.5c0 4.6 3.2 8.2 7.5 9.5 4.3-1.3 7.5-4.9 7.5-9.5V6z"/><rect x="9" y="10.5" width="6" height="5" rx="1"/><path d="M10.2 10.5V9a1.8 1.8 0 0 1 3.6 0v1.5"/></symbol><symbol id="i-fbin" viewBox="0 0 24 24"><path d="M6.5 3h7.5l4.5 4.5v12a1.5 1.5 0 0 1-1.5 1.5h-10.5a1.5 1.5 0 0 1-1.5-1.5v-15a1.5 1.5 0 0 1 1.5-1.5z"/><path d="M14 3v4.5h4.5"/><path d="M8.5 12.5v4M11 12.5h1.5v4H11zM15.5 12.5v4"/></symbol><symbol id="i-home" viewBox="0 0 24 24"><path d="M3 11.5 12 4l9 7.5"/><path d="M5.5 10v9.5h13V10"/><path d="M10 19.5v-5h4v5"/></symbol><symbol id="i-lock" viewBox="0 0 24 24"><rect x="5.5" y="10.5" width="13" height="9" rx="2"/><path d="M8.5 10.5V8a3.5 3.5 0 0 1 7 0v2.5"/></symbol></defs></svg>';
  var NAV = {
    it: { home: 'Inizio', flash: 'Installa', store: 'Pacchetti', lic: 'Tecnologie e licenze', tema: 'Tema', auto: 'Automatico', light: 'Chiaro', dark: 'Scuro', a11y: 'Accessibilita', hc: 'Contrasto alto', cb: 'Colori per daltonici', fs: 'Testo grande', rm: 'Meno animazioni', reset: 'Ripristina', lingua: 'Lingua' },
    en: { home: 'Home', flash: 'Install', store: 'Packages', lic: 'Technologies and licenses', tema: 'Theme', auto: 'Automatic', light: 'Light', dark: 'Dark', a11y: 'Accessibility', hc: 'High contrast', cb: 'Colour-blind colours', fs: 'Large text', rm: 'Less motion', reset: 'Reset', lingua: 'Language' },
    es: { home: 'Inicio', flash: 'Instalar', store: 'Paquetes', lic: 'Tecnologias y licencias', tema: 'Tema', auto: 'Automatico', light: 'Claro', dark: 'Oscuro', a11y: 'Accesibilidad', hc: 'Contraste alto', cb: 'Colores para daltonicos', fs: 'Texto grande', rm: 'Menos animaciones', reset: 'Restablecer', lingua: 'Idioma' },
    de: { home: 'Start', flash: 'Installieren', store: 'Pakete', lic: 'Technik und Lizenzen', tema: 'Design', auto: 'Automatisch', light: 'Hell', dark: 'Dunkel', a11y: 'Barrierefreiheit', hc: 'Hoher Kontrast', cb: 'Farben fuer Farbfehlsichtige', fs: 'Grosse Schrift', rm: 'Weniger Bewegung', reset: 'Zuruecksetzen', lingua: 'Sprache' }
  };
  var NOMI = { it: 'Italiano', en: 'English', es: 'Español', de: 'Deutsch' };
  var base = new URL('..', document.currentScript.src).href;   // radice del sito
  var S = { lang: 'it', dizionari: [] };
  var THEME = 'auto', A11Y = { fs: 0, hc: 'auto', cb: 0, rm: 'auto' };

  function leggi(k, d) { try { var v = localStorage.getItem(k); return v === null ? d : v; } catch (e) { return d; } }
  function scrivi(k, v) { try { localStorage.setItem(k, v); } catch (e) { /* senza memoria del browser la scelta vale solo per questa visita */ } }
  function $(id) { return document.getElementById(id); }

  S.lingue = NOMI;
  S.tr = function (k) {
    for (var i = S.dizionari.length - 1; i >= 0; i--) {
      var d = S.dizionari[i][S.lang] || S.dizionari[i].it;
      if (d && d[k] !== undefined) return d[k];
      if (S.dizionari[i].it && S.dizionari[i].it[k] !== undefined) return S.dizionari[i].it[k];
    }
    var n = NAV[S.lang] || NAV.it;
    return n[k] !== undefined ? n[k] : k;
  };
  S.add = function (d) { S.dizionari.push(d); };
  S.apply = function () {
    if (document.body.getAttribute('data-propria')) return;       // la pagina di installazione traduce da sola i suoi testi
    document.documentElement.lang = S.lang;
    var tt = document.querySelectorAll('[data-t]');
    for (var i = 0; i < tt.length; i++) tt[i].textContent = S.tr(tt[i].getAttribute('data-t'));
    var al = document.querySelectorAll('[data-al]');
    for (var j = 0; j < al.length; j++) al[j].setAttribute('aria-label', S.tr(al[j].getAttribute('data-al')));
    var tk = document.body.getAttribute('data-titolo');
    if (tk) document.title = S.tr(tk);
  };
  S.setLang = function (c) {
    if (!NAV[c]) c = 'it';
    S.lang = c; scrivi('vl', c);
    disegnaBarra(); S.apply();
    var sel = $('lingua-sito'); if (sel) sel.value = c;
    document.dispatchEvent(new CustomEvent('sito-lang'));
  };

  // ---- tema e accessibilita (stessa logica della pagina della scheda) ----
  function mm(q) { return !!(window.matchMedia && matchMedia(q).matches); }
  function temaApplica() {
    var m = THEME;
    if (m === 'auto') m = mm('(prefers-color-scheme: light)') ? 'light' : 'dark';
    document.documentElement.setAttribute('data-theme', m);
  }
  function a11yApplica() {
    var d = document.documentElement;
    d.setAttribute('data-fs', A11Y.fs);
    d.setAttribute('data-hc', (A11Y.hc === 'on' || (A11Y.hc === 'auto' && mm('(prefers-contrast: more)'))) ? '1' : '0');
    d.setAttribute('data-cb', A11Y.cb ? '1' : '0');
    d.setAttribute('data-rm', (A11Y.rm === 'on' || (A11Y.rm === 'auto' && mm('(prefers-reduced-motion: reduce)'))) ? '1' : '0');
  }
  function a11ySalva() { scrivi('va11y', JSON.stringify(A11Y)); a11yApplica(); disegnaBarra(); }
  function chiudi() { var p = document.querySelectorAll('.pop'); for (var i = 0; i < p.length; i++) p[i].classList.add('hide'); }
  function apri(id) { var p = $(id), era = p.classList.contains('hide'); chiudi(); if (era) p.classList.remove('hide'); }

  function icona(id) { return '<svg class="i" aria-hidden="true"><use href="#' + id + '"/></svg>'; }

  function disegnaBarra() {
    var n = NAV[S.lang] || NAV.it, host = $('nav-sito');
    if (!host) return;
    var pg = document.body.getAttribute('data-pg') || '';
    var voci = [['home', 'index.html'], ['flash', 'flash/'], ['store', 'store/'], ['lic', 'licenze/']];
    var h = document.querySelector('a.salta:not(.mia)') ? '' : '<a class="salta mia" href="#principale">' + (S.lang === 'it' ? 'Vai al contenuto' : S.lang === 'en' ? 'Skip to content' : S.lang === 'es' ? 'Ir al contenido' : 'Zum Inhalt') + '</a>';
    h += '<div class="top"><a class="marca" href="' + base + 'index.html"><svg class="logo" aria-hidden="true"><use href="#i-chip"/></svg><span>VesevOS</span></a><nav aria-label="VesevOS">';
    for (var i = 0; i < voci.length; i++) h += '<a href="' + base + voci[i][1] + '"' + (pg === voci[i][0] ? ' class="on" aria-current="page"' : '') + '>' + n[voci[i][0]] + '</a>';
    h += '</nav><span class="sp"></span>';
    if (!$('lingua')) {
      h += '<label class="salta" for="lingua-sito">' + n.lingua + '</label><select id="lingua-sito" aria-label="' + n.lingua + '">';
      for (var c in NOMI) h += '<option value="' + c + '"' + (c === S.lang ? ' selected' : '') + '>' + NOMI[c] + '</option>';
      h += '</select>';
    }
    var ti = THEME === 'auto' ? 'i-auto' : THEME === 'light' ? 'i-sun' : 'i-moon';
    h += '<button class="ib" id="thb" type="button" aria-haspopup="true" aria-label="' + n.tema + ': ' + n[THEME] + '" title="' + n.tema + ': ' + n[THEME] + '">' + icona(ti) + '</button>';
    h += '<button class="ib" id="a11b" type="button" aria-haspopup="true" aria-label="' + n.a11y + '" title="' + n.a11y + '">' + icona('i-a11y') + '</button></div>';
    h += '<div class="pop hide" id="thm" role="menu">';
    var tm = [['auto', 'i-auto'], ['light', 'i-sun'], ['dark', 'i-moon']];
    for (var k = 0; k < tm.length; k++) h += '<button type="button" role="menuitemradio" data-tema="' + tm[k][0] + '" class="' + (THEME === tm[k][0] ? 'on' : '') + '" aria-checked="' + (THEME === tm[k][0]) + '">' + icona(tm[k][1]) + n[tm[k][0]] + '<span class="ck" aria-hidden="true">✓</span></button>';
    h += '</div><div class="pop hide" id="a11m" role="menu"><h4>' + n.a11y + '</h4>';
    var ac = [['hc', n.hc, A11Y.hc === 'on'], ['cb', n.cb, !!A11Y.cb], ['fs', n.fs, +A11Y.fs > 0], ['rm', n.rm, A11Y.rm === 'on']];
    for (var q = 0; q < ac.length; q++) h += '<button type="button" role="menuitemcheckbox" data-a11y="' + ac[q][0] + '" class="' + (ac[q][2] ? 'on' : '') + '" aria-checked="' + ac[q][2] + '">' + ac[q][1] + '<span class="ck" aria-hidden="true">✓</span></button>';
    h += '<button type="button" data-a11y="reset">' + n.reset + '</button></div>';
    host.innerHTML = h;
    $('thb').onclick = function (e) { e.stopPropagation(); apri('thm'); };
    $('a11b').onclick = function (e) { e.stopPropagation(); apri('a11m'); };
    var bt = host.querySelectorAll('[data-tema]');
    for (var a = 0; a < bt.length; a++) bt[a].onclick = function () { THEME = this.getAttribute('data-tema'); scrivi('vth', THEME); temaApplica(); disegnaBarra(); };
    var ba = host.querySelectorAll('[data-a11y]');
    for (var b = 0; b < ba.length; b++) ba[b].onclick = function () {
      var k2 = this.getAttribute('data-a11y');
      if (k2 === 'reset') A11Y = { fs: 0, hc: 'auto', cb: 0, rm: 'auto' };
      else if (k2 === 'hc') A11Y.hc = A11Y.hc === 'on' ? 'off' : 'on';
      else if (k2 === 'cb') A11Y.cb = A11Y.cb ? 0 : 1;
      else if (k2 === 'fs') A11Y.fs = +A11Y.fs > 0 ? 0 : 1;
      else if (k2 === 'rm') A11Y.rm = A11Y.rm === 'on' ? 'off' : 'on';
      a11ySalva();
    };
    var sel = $('lingua-sito');
    if (sel) sel.onchange = function () { S.setLang(sel.value); };
  }

  function avvia() {
    document.body.insertAdjacentHTML('afterbegin', SPRITE);
    THEME = leggi('vth', 'auto');
    try { var a = JSON.parse(leggi('va11y', '{}')); for (var k in A11Y) if (a[k] !== undefined) A11Y[k] = a[k]; } catch (e) { /* impostazioni rovinate: valori di base */ }
    var l = leggi('vl', (navigator.language || 'it').slice(0, 2).toLowerCase());
    S.lang = NAV[l] ? l : 'it';
    temaApplica(); a11yApplica(); disegnaBarra(); S.apply();
    document.addEventListener('click', function (e) { if (!e.target.closest || !e.target.closest('.pop')) chiudi(); });
    document.addEventListener('keydown', function (e) { if (e.key === 'Escape') chiudi(); });
    if (window.matchMedia) matchMedia('(prefers-color-scheme: light)').addEventListener('change', function () { if (THEME === 'auto') temaApplica(); });
    document.dispatchEvent(new CustomEvent('sito-pronto'));
  }
  window.SITO = S;
  if (document.readyState === 'complete') avvia(); else document.addEventListener('DOMContentLoaded', avvia);
})();
