// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// Home del sito: scelta della lingua (it, en, es, de), tema automatico/chiaro/scuro e menu di accessibilita.
// Stessa memoria delle altre pagine del sito (chiavi vl, vth, va11y): la scelta vale per tutto il sito.
// Nessun dato esce dal browser. I testi italiani sono quelli della pagina; qui stanno le altre tre lingue.
(function () {
  'use strict';
  var TR = {"Vai al contenuto": ["Skip to content", "Ir al contenido", "Zum Inhalt springen"], "Funzioni": ["Features", "Funciones", "Funktionen"], "La pagina": ["The page", "La página", "Die Seite"], "Stato": ["Status", "Estado", "Status"], "CPU, RAM e temperatura": ["CPU, RAM and temperature", "CPU, RAM y temperatura", "CPU, RAM und Temperatur"], "Automazioni": ["Automations", "Automatizaciones", "Automatisierungen"], "Regole Quando, Se, Allora": ["“When, If, Then” rules", "Reglas Cuando, Si, Entonces", "Regeln Wenn, Falls, Dann"], "Lo schema della scheda": ["The board diagram", "El esquema de la placa", "Der Plan der Platine"], "Per cominciare": ["Getting started", "Para empezar", "Erste Schritte"], "Prima accensione": ["First power-up", "Primer encendido", "Erster Start"], "Premi BOOT e parti": ["Press BOOT and go", "Pulsa BOOT y arranca", "BOOT drücken und los"], "Novità": ["What's new", "Novedades", "Neuheiten"], "La versione 1.8.24c": ["Version 1.8.24c", "La versión 1.8.24c", "Die Version 1.8.24c"], "Pagina": ["Page", "Página", "Seite"], "Avvio": ["Setup", "Inicio", "Start"], "Domande": ["Questions", "Preguntas", "Fragen"], "Installa dal browser": ["Install from your browser", "Instalar desde el navegador", "Im Browser installieren"], "Versione 1.8.24c, ora anche per ESP32-C6": ["Version 1.8.24c, now also for ESP32-C6", "Versión 1.8.24c, ahora también para ESP32-C6", "Version 1.8.24c, jetzt auch für ESP32-C6"], "Sistema operativo per ESP32-S3 e C6": ["Operating system for ESP32-S3 and C6", "Sistema operativo para ESP32-S3 y C6", "Betriebssystem für ESP32-S3 und C6"], "Pagina web, shell, driver e programmi sulla tua scheda: si apre dal browser, parla quattro lingue e funziona senza cloud né account.": ["Web page, shell, drivers and programs on your board: it opens in the browser, speaks four languages and works with no cloud and no account.", "Página web, shell, controladores y programas en tu placa: se abre en el navegador, habla cuatro idiomas y funciona sin nube ni cuenta.", "Webseite, Shell, Treiber und Programme auf deiner Platine: Sie öffnet sich im Browser, spricht vier Sprachen und funktioniert ohne Cloud und ohne Konto."], "Scarica il firmware": ["Download the firmware", "Descargar el firmware", "Firmware herunterladen"], "Schede": ["Boards", "Placas", "Platinen"], "S3 e C6": ["S3 and C6", "S3 y C6", "S3 und C6"], "Lingue": ["Languages", "Idiomas", "Sprachen"], "Licenza": ["License", "Licencia", "Lizenz"], "GPL-3.0 o commerciale": ["GPL-3.0 or commercial", "GPL-3.0 o comercial", "GPL-3.0 oder kommerziell"], "Programmi e servizi sotto controllo.": ["Programs and services under control.", "Programas y servicios bajo control.", "Programme und Dienste unter Kontrolle."], "Avvia, ferma e riavvia.": ["Start, stop and restart.", "Inicia, detén y reinicia.", "Starten, stoppen und neu starten."], "Apri Task dal browser: sottosistemi e programmi in esecuzione sono lì, con i pulsanti per avviare, fermare e riavviare.": ["Open Tasks in your browser: subsystems and running programs are right there, with buttons to start, stop and restart.", "Abre Tareas en el navegador: subsistemas y programas en ejecución están ahí, con botones para iniciar, detener y reiniciar.", "Öffne Tasks im Browser: Subsysteme und laufende Programme sind dort, mit Schaltflächen zum Starten, Stoppen und Neustarten."], "Guarda le altre viste": ["See the other views", "Ver las otras vistas", "Weitere Ansichten ansehen"], "Automazioni con Quando, Se, Allora.": ["Automations with When, If, Then.", "Automatizaciones con Cuando, Si, Entonces.", "Automatisierungen mit Wenn, Falls, Dann."], "Scrivi una regola in tre parti: quando succede qualcosa, se vale una condizione, allora la scheda agisce, anche senza rete.": ["Write a rule in three parts: when something happens, if a condition holds, then the board acts, even with no network.", "Escribe una regla en tres partes: cuando ocurre algo, si se cumple una condición, entonces la placa actúa, incluso sin red.", "Schreibe eine Regel in drei Teilen: wenn etwas passiert, falls eine Bedingung gilt, dann handelt die Platine, auch ohne Netz."], "Scopri l'installazione guidata": ["Discover the guided installation", "Descubre la instalación guiada", "Die geführte Installation entdecken"], "Alla prima accensione parte l'installazione guidata: scegli lingua, password e rete, un passo dopo l'altro.": ["At first power-up the guided installation starts: choose language, password and network, one step at a time.", "En el primer encendido arranca la instalación guiada: elige idioma, contraseña y red, paso a paso.", "Beim ersten Start beginnt die geführte Installation: Sprache, Passwort und Netz wählen, Schritt für Schritt."], "Due viste per tenere tutta la scheda sotto controllo.": ["Two views to keep the whole board under control.", "Dos vistas para tener toda la placa bajo control.", "Zwei Ansichten, um die ganze Platine im Griff zu haben."], "Servizi e pin a portata di clic.": ["Services and pins a click away.", "Servicios y pines a un clic.", "Dienste und Pins nur einen Klick entfernt."], "Ordina l'avvio dei servizi": ["Set the startup order of services", "Ordena el arranque de los servicios", "Startreihenfolge der Dienste festlegen"], "Trascina i servizi per la maniglia o usa le frecce: quello che dipende da un altro parte dopo di lui.": ["Drag services by the handle or use the arrows: whatever depends on another starts after it.", "Arrastra los servicios por el asa o usa las flechas: lo que depende de otro arranca después.", "Ziehe die Dienste am Griff oder nutze die Pfeile: was von einem anderen abhängt, startet danach."], "Controlla ogni pin": ["Check every pin", "Controla cada pin", "Jeden Pin prüfen"], "Tocca un pin verde per provarlo: blu è in uso, grigio è vietato. Lo schema c'è sul fronte e sul retro.": ["Tap a green pin to test it: blue is in use, grey is forbidden. The diagram covers front and back.", "Toca un pin verde para probarlo: azul está en uso, gris está prohibido. El esquema muestra anverso y reverso.", "Tippe auf einen grünen Pin zum Testen: blau ist belegt, grau ist gesperrt. Der Plan zeigt Vorder- und Rückseite."], "Dalla scatola alla pagina in pochi minuti.": ["From the box to the page in a few minutes.", "De la caja a la página en pocos minutos.", "Vom Karton zur Seite in wenigen Minuten."], "Premi BOOT, collegati alla rete VesevOS e apri 192.168.4.1: poi scegli la password e segui la guida.": ["Press BOOT, join the VesevOS network and open 192.168.4.1: then choose the password and follow the guide.", "Pulsa BOOT, conéctate a la red VesevOS y abre 192.168.4.1: luego elige la contraseña y sigue la guía.", "BOOT drücken, mit dem Netz VesevOS verbinden und 192.168.4.1 öffnen: dann Passwort wählen und der Anleitung folgen."], "Apri la pagina di installazione": ["Open the installation page", "Abre la página de instalación", "Installationsseite öffnen"], "Novità della 1.8.24c.": ["What's new in 1.8.24c.", "Novedades de la 1.8.24c.", "Neuheiten in 1.8.24c."], "Una scheda in più, un sistema più leggero.": ["One more board, a lighter system.", "Una placa más, un sistema más ligero.", "Eine Platine mehr, ein schlankeres System."], "VesevOS gira anche su ESP32-C6. Pagina, manuale, lingue e licenze sono file propri sulla scheda, ognuno controllato con SHA-256.": ["VesevOS now also runs on ESP32-C6. Page, manual, languages and licenses are files of their own on the board, each one checked with SHA-256.", "VesevOS ahora también funciona en ESP32-C6. Página, manual, idiomas y licencias son archivos propios en la placa, cada uno comprobado con SHA-256.", "VesevOS läuft jetzt auch auf dem ESP32-C6. Seite, Handbuch, Sprachen und Lizenzen sind eigene Dateien auf der Platine, jede mit SHA-256 geprüft."], "Domande.": ["Questions.", "Preguntas.", "Fragen."], "Altro da sapere? La guida completa è sul sito del progetto.": ["Want to know more? The complete guide is on the project site.", "¿Quieres saber más? La guía completa está en el sitio del proyecto.", "Mehr wissen? Die vollständige Anleitung steht auf der Projektseite."], "Leggi la guida completa": ["Read the complete guide", "Lee la guía completa", "Vollständige Anleitung lesen"], "Quali schede sono supportate?": ["Which boards are supported?", "¿Qué placas son compatibles?", "Welche Platinen werden unterstützt?"], "ESP32-S3 ed ESP32-C6. L'installazione dal browser è pronta per la S3; la C6 si aggiorna con il file.": ["ESP32-S3 and ESP32-C6. Installation from the browser is ready for the S3; the C6 updates with the file.", "ESP32-S3 y ESP32-C6. La instalación desde el navegador está lista para la S3; la C6 se actualiza con el archivo.", "ESP32-S3 und ESP32-C6. Die Installation im Browser ist für den S3 bereit; der C6 wird mit der Datei aktualisiert."], "La pagina funziona senza internet?": ["Does the page work without internet?", "¿La página funciona sin internet?", "Funktioniert die Seite ohne Internet?"], "Sì. Pagina, manuale e lingue stanno sulla scheda.": ["Yes. The page, manual and languages live on the board.", "Sí. La página, el manual y los idiomas están en la placa.", "Ja. Seite, Handbuch und Sprachen liegen auf der Platine."], "Come si aggiorna la scheda?": ["How do I update the board?", "¿Cómo se actualiza la placa?", "Wie aktualisiere ich die Platine?"], "Con Aggiorna tramite recovery o con Carica da file.": ["With Update through recovery or with Load from file.", "Con Actualizar mediante recovery o con Cargar desde archivo.", "Mit „Über das Recovery aktualisieren“ oder mit „Aus Datei laden“."], "Quali lingue parla?": ["Which languages does it speak?", "¿Qué idiomas habla?", "Welche Sprachen spricht es?"], "Italiano, inglese, spagnolo e tedesco.": ["Italian, English, Spanish and German.", "Italiano, inglés, español y alemán.", "Italienisch, Englisch, Spanisch und Deutsch."], "Prova VesevOS su una scheda": ["Try VesevOS on a board", "Prueba VesevOS en una placa", "Probiere VesevOS auf einer Platine aus"], "Si apre dal browser, in quattro lingue.": ["It opens in the browser, in four languages.", "Se abre en el navegador, en cuatro idiomas.", "Öffnet sich im Browser, in vier Sprachen."], "Il sistema per la tua scheda ESP32.": ["The system for your ESP32 board.", "El sistema para tu placa ESP32.", "Das System für deine ESP32-Platine."], "Scopri": ["Discover", "Descubre", "Entdecken"], "Parti": ["Start", "Empieza", "Los geht's"], "Progetto": ["Project", "Proyecto", "Projekt"], "Codice sorgente": ["Source code", "Código fuente", "Quellcode"], "Pacchetti": ["Packages", "Paquetes", "Pakete"], "Strumenti": ["Tools", "Herramientas", "Werkzeuge"], "© 2026 VesevOS GPL-3.0 o licenza commerciale GitHub, Facebook e i loro loghi sono marchi dei rispettivi proprietari": ["© 2026 VesevOS GPL-3.0 or commercial license GitHub, Facebook and their logos are trademarks of their respective owners", "© 2026 VesevOS GPL-3.0 o licencia comercial GitHub, Facebook y sus logotipos son marcas de sus respectivos propietarios", "© 2026 VesevOS GPL-3.0 oder kommerzielle Lizenz GitHub, Facebook und ihre Logos sind Marken ihrer jeweiligen Inhaber"], "Gruppo Facebook": ["Facebook group", "Grupo de Facebook", "Facebook-Gruppe"], "Crediti: font, icone e strumenti": ["Credits: fonts, icons and tools", "Créditos: fuentes, iconos y herramientas", "Nachweise: Schriften, Icons und Werkzeuge"], "Principale": ["Main", "Principal", "Hauptmenü"], "Menu": ["Menu", "Menú", "Menü"], "Azione rapida": ["Quick action", "Acción rápida", "Schnellaktion"], "La Home della pagina VesevOS: anelli di stato di CPU, RAM, PSRAM, file e Wi-Fi, e le informazioni del sistema": ["The VesevOS Home page: status rings for CPU, RAM, PSRAM, files and Wi-Fi, and system information", "La Home de la página VesevOS: anillos de estado de CPU, RAM, PSRAM, archivos y Wi-Fi, e información del sistema", "Die Startseite von VesevOS: Statusringe für CPU, RAM, PSRAM, Dateien und WLAN sowie Systeminformationen"], "La pagina Task: i sottosistemi con i pulsanti per avviare, fermare e riavviare": ["The Tasks page: subsystems with buttons to start, stop and restart", "La página Tareas: subsistemas con botones para iniciar, detener y reiniciar", "Die Tasks-Seite: Subsysteme mit Schaltflächen zum Starten, Stoppen und Neustarten"], "La pagina Automazioni: una regola Sveglia con i modelli pronti": ["The Automations page: a Wake-up rule with ready-made templates", "La página Automatizaciones: una regla Despertador con plantillas listas", "Die Automatisierungs-Seite: eine Regel Wecker mit fertigen Vorlagen"], "Ordine di avvio: i servizi sempre per primi e quelli che puoi spostare con le frecce": ["Startup order: services that always go first and those you can move with the arrows", "Orden de arranque: los servicios que van siempre primero y los que puedes mover con las flechas", "Startreihenfolge: Dienste, die immer zuerst laufen, und solche, die du mit den Pfeilen verschieben kannst"], "Lo schema dei pin della scheda, fronte e retro, con i pin provabili, in uso e vietati": ["The board's pin diagram, front and back, with testable, in-use and forbidden pins", "El esquema de pines de la placa, anverso y reverso, con pines probables, en uso y prohibidos", "Der Pinplan der Platine, Vorder- und Rückseite, mit testbaren, belegten und gesperrten Pins"], "La pagina della scheda sul telefono, con la barra delle icone in basso": ["The board page on a phone, with the icon bar at the bottom", "La página de la placa en el teléfono, con la barra de iconos abajo", "Die Seite der Platine auf dem Telefon, mit der Symbolleiste unten"], "VesevOS: sistema operativo per ESP32-S3 ed ESP32-C6": ["VesevOS: operating system for ESP32-S3 and ESP32-C6", "VesevOS: sistema operativo para ESP32-S3 y ESP32-C6", "VesevOS: Betriebssystem für ESP32-S3 und ESP32-C6"], "Una pagina web, una shell, driver e programmi sulla tua scheda: si apre dal browser, parla quattro lingue e non chiede cloud né account.": ["A web page, a shell, drivers and programs on your board: it opens in the browser, speaks four languages and needs no cloud or account.", "Una página web, una shell, controladores y programas en tu placa: se abre en el navegador, habla cuatro idiomas y no pide nube ni cuenta.", "Eine Webseite, eine Shell, Treiber und Programme auf deiner Platine: Sie öffnet sich im Browser, spricht vier Sprachen und braucht weder Cloud noch Konto."]};
  var CRED = {"it": "Font <a href=\"https://github.com/googlefonts/onest\">Onest</a> (The Onest Project Authors) e <a href=\"https://github.com/JetBrains/JetBrainsMono\">JetBrains Mono</a> (JetBrains), licenza SIL OFL 1.1. Icone <a href=\"https://github.com/lucide-icons/lucide\">Lucide</a> (Lucide Contributors, ISC) e <a href=\"https://github.com/simple-icons/simple-icons\">Simple Icons</a> (CC0 1.0).", "en": "Fonts <a href=\"https://github.com/googlefonts/onest\">Onest</a> (The Onest Project Authors) and <a href=\"https://github.com/JetBrains/JetBrainsMono\">JetBrains Mono</a> (JetBrains), SIL OFL 1.1 license. Icons <a href=\"https://github.com/lucide-icons/lucide\">Lucide</a> (Lucide Contributors, ISC) and <a href=\"https://github.com/simple-icons/simple-icons\">Simple Icons</a> (CC0 1.0).", "es": "Fuentes <a href=\"https://github.com/googlefonts/onest\">Onest</a> (The Onest Project Authors) y <a href=\"https://github.com/JetBrains/JetBrainsMono\">JetBrains Mono</a> (JetBrains), licencia SIL OFL 1.1. Iconos <a href=\"https://github.com/lucide-icons/lucide\">Lucide</a> (Lucide Contributors, ISC) y <a href=\"https://github.com/simple-icons/simple-icons\">Simple Icons</a> (CC0 1.0).", "de": "Schriften <a href=\"https://github.com/googlefonts/onest\">Onest</a> (The Onest Project Authors) und <a href=\"https://github.com/JetBrains/JetBrainsMono\">JetBrains Mono</a> (JetBrains), Lizenz SIL OFL 1.1. Icons <a href=\"https://github.com/lucide-icons/lucide\">Lucide</a> (Lucide Contributors, ISC) und <a href=\"https://github.com/simple-icons/simple-icons\">Simple Icons</a> (CC0 1.0)."};
  var MOSTRA = {"it": ["Mostra", "Chiudi"], "en": ["Show", "Hide"], "es": ["Mostrar", "Cerrar"], "de": ["Anzeigen", "Schließen"]};
  var IDX = { en: 0, es: 1, de: 2 };
  var NOMI = { it: 'Italiano', en: 'English', es: 'Español', de: 'Deutsch' };
  var S = null;
  var tema = 'auto';
  var a11y = { fs: 0, hc: 'auto', cb: 0, rm: 'auto' };
  var testi = new WeakMap();      // nodo di testo -> frase italiana originale
  var attributi = [];             // { el, attr, it }
  var html = document.documentElement;

  function leggi(k, d) { try { var v = localStorage.getItem(k); return v === null ? d : v; } catch (e) { return d; } }
  function scrivi(k, v) { try { localStorage.setItem(k, v); } catch (e) { /* senza memoria la scelta vale solo per questa visita */ } }
  function norm(t) { return t.replace(/\s+/g, ' ').trim(); }
  function mm(q) { return !!(window.matchMedia && matchMedia(q).matches); }
  function lingua() { return (S && S.lang) || 'it'; }
  function ui(k) { return S ? S.tr(k) : k; }
  function $(id) { return document.getElementById(id); }

  // ---- traduzione dei testi ----
  function frase(it, l) {
    if (l === 'it' || !TR[it]) return it;
    var v = TR[it][IDX[l]];
    return v === undefined ? it : v;
  }

  function traduciNodi(l) {
    var radice = document.body;
    var w = document.createTreeWalker(radice, NodeFilter.SHOW_TEXT, {
      acceptNode: function (n) {
        var p = n.parentNode;
        if (!p || /^(SCRIPT|STYLE|NOSCRIPT)$/.test(p.nodeName)) return NodeFilter.FILTER_REJECT;
        if (p.closest && (p.closest('svg') || p.closest('#premi-h') || p.closest('#crediti-testo'))) return NodeFilter.FILTER_REJECT;
        return NodeFilter.FILTER_ACCEPT;
      }
    });
    var n;
    while ((n = w.nextNode())) {
      var it = testi.get(n);
      if (it === undefined) {
        var c = norm(n.nodeValue);
        if (!c || !TR[c]) continue;
        it = c; testi.set(n, it);
      }
      var m = n.nodeValue.match(/^(\s*)[\s\S]*?(\s*)$/);
      n.nodeValue = m[1] + frase(it, l) + m[2];
    }
  }

  function traduciAttributi(l) {
    if (!attributi.length) {
      var els = document.querySelectorAll('[alt],[aria-label],[title]');
      for (var i = 0; i < els.length; i++) {
        ['alt', 'aria-label', 'title'].forEach(function (a) {
          var v = els[i].getAttribute(a);
          if (v && TR[v] && !els[i].closest('#ctl-home')) attributi.push({ el: els[i], attr: a, it: v });
        });
      }
    }
    attributi.forEach(function (x) { x.el.setAttribute(x.attr, frase(x.it, l)); });
  }

  var testata = { titolo: 'VesevOS: sistema operativo per ESP32-S3 ed ESP32-C6' };
  function traduciTesta(l) {
    document.title = frase(testata.titolo, l);
    var d = document.querySelector('meta[name="description"]');
    if (d) { if (!d.dataset.it) d.dataset.it = d.getAttribute('content'); d.setAttribute('content', frase(d.dataset.it, l)); }
    var ot = document.querySelector('meta[property="og:title"]'); if (ot) ot.setAttribute('content', frase(testata.titolo, l));
  }

  function traduciSpeciali(l) {
    var h = $('premi-h');
    if (h) { h.textContent = frase('Premi BOOT e parti', l); h.removeAttribute('class'); h.classList.add('mo-la__type'); }
    var c = $('crediti-testo');
    if (c) c.innerHTML = CRED[l] || CRED.it;
    var cr = document.querySelector('.credits');
    if (cr) { cr.setAttribute('data-mostra', MOSTRA[l][0]); cr.setAttribute('data-chiudi', MOSTRA[l][1]); }
  }

  function traduci() {
    var l = lingua();
    html.lang = l;
    traduciNodi(l); traduciAttributi(l); traduciTesta(l); traduciSpeciali(l);
    disegnaControlli();
  }

  // ---- tema e accessibilita ----
  function temaApplica() {
    var m = tema === 'auto' ? (mm('(prefers-color-scheme: light)') ? 'light' : 'dark') : tema;
    html.setAttribute('data-theme', m);
  }
  function a11yApplica() {
    html.setAttribute('data-fs', a11y.fs);
    html.setAttribute('data-hc', (a11y.hc === 'on' || (a11y.hc === 'auto' && mm('(prefers-contrast: more)'))) ? '1' : '0');
    html.setAttribute('data-cb', a11y.cb ? '1' : '0');
    html.setAttribute('data-rm', (a11y.rm === 'on' || (a11y.rm === 'auto' && mm('(prefers-reduced-motion: reduce)'))) ? '1' : '0');
  }

  // ---- controlli nella barra ----
  function icona(id) { return '<svg class="i" aria-hidden="true" width="22" height="22"><use href="#' + id + '"/></svg>'; }
  function chiudi() { var p = document.querySelectorAll('.pop'); for (var i = 0; i < p.length; i++) p[i].classList.add('hide'); }
  function apri(id) { var p = $(id), era = p.classList.contains('hide'); chiudi(); if (era) p.classList.remove('hide'); }

  function costruisci() {
    var az = document.querySelector('.nav__actions');
    if (!az || $('ctl-home')) return;
    var d = document.createElement('div');
    d.className = 'ctl'; d.id = 'ctl-home';
    d.innerHTML = '<label for="lingua-home" id="lbl-lingua" class="lbl-ctl"></label><select id="lingua-home"></select>' +
      '<button class="ib" id="tema-btn" type="button" aria-haspopup="true"></button>' +
      '<button class="ib" id="a11y-btn" type="button" aria-haspopup="true"></button>';
    az.insertBefore(d, az.firstChild);
    var p1 = document.createElement('div'); p1.className = 'pop hide'; p1.id = 'tema-pop'; p1.setAttribute('role', 'menu');
    var p2 = document.createElement('div'); p2.className = 'pop hide'; p2.id = 'a11y-pop'; p2.setAttribute('role', 'menu');
    document.body.appendChild(p1); document.body.appendChild(p2);
    $('tema-btn').onclick = function (e) { e.stopPropagation(); apri('tema-pop'); };
    $('a11y-btn').onclick = function (e) { e.stopPropagation(); apri('a11y-pop'); };
    $('lingua-home').onchange = function () { if (S) S.setLang(this.value); traduci(); };
    document.addEventListener('click', function (e) { if (!e.target.closest || !e.target.closest('.pop')) chiudi(); });
    document.addEventListener('keydown', function (e) { if (e.key === 'Escape') chiudi(); });
  }

  function disegnaControlli() {
    if (!$('ctl-home')) return;
    var l = lingua();
    $('lbl-lingua').textContent = ui('lingua');
    var sel = $('lingua-home');
    if (!sel.options.length) {
      for (var c in NOMI) { var o = document.createElement('option'); o.value = c; o.textContent = NOMI[c]; sel.appendChild(o); }
    }
    sel.value = l;
    var ti = tema === 'auto' ? 'i-auto' : tema === 'light' ? 'i-sun' : 'i-moon';
    var tb = $('tema-btn'); tb.innerHTML = icona(ti);
    tb.setAttribute('aria-label', ui('tema') + ': ' + ui(tema)); tb.title = ui('tema') + ': ' + ui(tema);
    var ab = $('a11y-btn'); ab.innerHTML = icona('i-a11y'); ab.setAttribute('aria-label', ui('a11y')); ab.title = ui('a11y');
    var h = '', tm = [['auto', 'i-auto'], ['light', 'i-sun'], ['dark', 'i-moon']];
    for (var k = 0; k < tm.length; k++) h += '<button type="button" role="menuitemradio" data-tema="' + tm[k][0] + '" class="' + (tema === tm[k][0] ? 'on' : '') + '" aria-checked="' + (tema === tm[k][0]) + '">' + icona(tm[k][1]) + ui(tm[k][0]) + '</button>';
    $('tema-pop').innerHTML = h;
    var ac = [['hc', ui('hc'), a11y.hc === 'on'], ['cb', ui('cb'), !!a11y.cb], ['fs', ui('fs'), +a11y.fs > 0], ['rm', ui('rm'), a11y.rm === 'on']];
    h = '<h4>' + ui('a11y') + '</h4>';
    for (var q = 0; q < ac.length; q++) h += '<button type="button" role="menuitemcheckbox" data-a11y="' + ac[q][0] + '" class="' + (ac[q][2] ? 'on' : '') + '" aria-checked="' + ac[q][2] + '">' + ac[q][1] + '</button>';
    h += '<button type="button" data-a11y="reset">' + ui('reset') + '</button>';
    $('a11y-pop').innerHTML = h;
    var bt = $('tema-pop').querySelectorAll('[data-tema]');
    for (var a = 0; a < bt.length; a++) bt[a].onclick = function () { tema = this.getAttribute('data-tema'); scrivi('vth', tema); temaApplica(); disegnaControlli(); };
    var ba = $('a11y-pop').querySelectorAll('[data-a11y]');
    for (var b = 0; b < ba.length; b++) ba[b].onclick = function () {
      var k2 = this.getAttribute('data-a11y');
      if (k2 === 'reset') a11y = { fs: 0, hc: 'auto', cb: 0, rm: 'auto' };
      else if (k2 === 'hc') a11y.hc = a11y.hc === 'on' ? 'off' : 'on';
      else if (k2 === 'cb') a11y.cb = a11y.cb ? 0 : 1;
      else if (k2 === 'fs') a11y.fs = +a11y.fs > 0 ? 0 : 1;
      else if (k2 === 'rm') a11y.rm = a11y.rm === 'on' ? 'off' : 'on';
      scrivi('va11y', JSON.stringify(a11y)); a11yApplica(); disegnaControlli();
    };
  }

  function avvia() {
    S = window.SITO || null;
    tema = leggi('vth', 'auto');
    try { var a = JSON.parse(leggi('va11y', '{}')); for (var k in a11y) if (a[k] !== undefined) a11y[k] = a[k]; } catch (e) { /* impostazioni rovinate: valori di base */ }
    temaApplica(); a11yApplica();
    costruisci();
    traduci();
    window.addEventListener('load', function () { setTimeout(traduci, 1500); });   // il diagramma si disegna dopo
    if (window.matchMedia) matchMedia('(prefers-color-scheme: light)').addEventListener('change', function () { if (tema === 'auto') temaApplica(); });
  }
  if (document.readyState === 'complete') avvia(); else document.addEventListener('DOMContentLoaded', avvia);
})();
