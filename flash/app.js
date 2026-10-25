// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
//
// Pagina della prima installazione. Nessun dato esce dal browser:
// si scarica solo da questo sito e si scrive sulla porta seriale scelta dall'utente.

import { LINGUE, impostaLingua, linguaIniziale, t, tf } from './lang.js';
import { controllaParte, tuttoOk } from './controlli.js';
import { scriviParti } from './scrittura.js';

const $ = (id) => document.getElementById(id);
const COLONNE = ['c_scarico', 'c_byte', 'c_marchio', 'c_dim', 'c_sha'];

let elenco = null;          // contenuto di firmware.json
let pacchettoAttuale = null;
let risultati = {};         // id parte -> { r, byte } oppure { errore }
let inScrittura = false;

// ---------- testi e lingua ----------

function traduci() {
  document.querySelectorAll('[data-t]').forEach((e) => { e.textContent = t(e.dataset.t); });
  document.documentElement.lang = $('lingua').value;
  document.title = t('titolo');
  disegnaPacchetti();
  disegnaTabella();
  aggiornaEsito();
}

function preparaLingua() {
  const sel = $('lingua');
  for (const [codice, nome] of Object.entries(LINGUE)) {
    sel.add(new Option(nome, codice));
  }
  sel.value = impostaLingua(linguaIniziale());
  sel.addEventListener('change', () => { impostaLingua(sel.value); traduci(); });
}

// ---------- registro e avanzamento ----------

function registra(testo, errore) {
  const li = document.createElement('li');
  li.textContent = testo;
  if (errore) li.className = 'errore';
  $('registro').appendChild(li);
  li.scrollIntoView({ block: 'nearest' });
}

function stato(testo, percento) {
  $('stato').textContent = testo;
  if (percento !== undefined) $('barra').value = percento;
}

// ---------- elenco dei pacchetti ----------

function nomePacchetto(p) {
  const tipo = t('tipo_' + p.tipo.replace('-', '_'));
  return tipo + ' - ' + tf('versione', p.versione);
}

function disegnaPacchetti() {
  const sel = $('pacchetto');
  const scelto = sel.value;
  sel.textContent = '';
  if (!elenco) return;
  for (const p of elenco.pacchetti) sel.add(new Option(nomePacchetto(p), p.id));
  if (scelto) sel.value = scelto;
}

function disegnaTabella() {
  const tab = $('tab-controlli');
  tab.textContent = '';
  if (!pacchettoAttuale) return;
  const capo = tab.createTHead().insertRow();
  for (const c of ['parte', ...COLONNE]) {
    const th = document.createElement('th');
    th.scope = 'col';
    th.textContent = t(c);
    capo.appendChild(th);
  }
  const corpo = tab.createTBody();
  for (const id of pacchettoAttuale.parti) {
    const parte = elenco.parti[id];
    const riga = corpo.insertRow();
    const nome = riga.insertCell();
    nome.textContent = t('tipo_' + parte.tipo) + ' (' + parte.file.split('/').pop() + ')';
    const res = risultati[id];
    COLONNE.forEach((col) => {
      const cella = riga.insertCell();
      let valore = null;                       // null = in attesa
      if (res && res.r) valore = col === 'c_scarico' ? true : res.r[col];
      if (res && res.errore) valore = col === 'c_scarico' ? false : null;
      cella.textContent = valore === null ? t('attesa') : (valore ? t('ok') : t('ko'));
      cella.className = valore === null ? 'att' : (valore ? 'ok' : 'ko');
    });
  }
}

// ---------- controlli ----------

function primoErrore() {
  for (const id of pacchettoAttuale.parti) {
    const res = risultati[id];
    if (res && res.errore) return res.errore;
    if (res && res.r && res.r.errore) return res.r.errore;
  }
  return null;
}

function pacchettoVerificato() {
  if (!pacchettoAttuale) return false;
  return pacchettoAttuale.parti.every((id) => risultati[id] && risultati[id].r && tuttoOk(risultati[id].r));
}

function aggiornaEsito() {
  const e = $('esito');
  e.className = 'esito';
  if (!pacchettoAttuale) { e.textContent = ''; }
  else if (pacchettoVerificato()) { e.textContent = t('tutti_ok'); e.classList.add('ok'); }
  else {
    const er = primoErrore();
    if (er) { e.textContent = tf(er.chiave, ...er.args); e.classList.add('ko'); }
    else { e.textContent = t('manca'); }
  }
  $('scrivi').disabled = inScrittura || !pacchettoVerificato() || !$('conferma').checked;
}

async function scaricaParte(id, parte) {
  const risposta = await fetch(parte.file, { cache: 'no-store', credentials: 'omit' });
  if (!risposta.ok) throw new Error('http ' + risposta.status);
  return new Uint8Array(await risposta.arrayBuffer());
}

async function verificaPacchetto() {
  risultati = {};
  $('conferma').checked = false;
  const id = $('pacchetto').value;
  pacchettoAttuale = elenco.pacchetti.find((p) => p.id === id) || null;
  disegnaTabella();
  aggiornaEsito();
  if (!pacchettoAttuale) return;
  const questo = pacchettoAttuale;
  if (!(window.crypto && crypto.subtle)) {
    risultati[questo.parti[0]] = { errore: { chiave: 'err_sha_non_disp', args: [] } };
    $('esito').textContent = t('err_sha_non_disp');
    $('esito').className = 'esito ko';
    return;
  }
  for (const pid of questo.parti) {
    const parte = elenco.parti[pid];
    if (!parte) { risultati[pid] = { errore: { chiave: 'err_contenuto', args: [pid] } }; continue; }
    stato(tf('f_scarico', parte.file), 0);
    try {
      const byte = await scaricaParte(pid, parte);
      if (questo !== pacchettoAttuale) return;       // l'utente ha cambiato scelta
      const r = await controllaParte(pid, parte, byte);
      risultati[pid] = { r, byte };
      if (!r.errore) registra(tf('f_sha_ok', parte.file));
      else registra(tf(r.errore.chiave, ...r.errore.args), true);
    } catch (e) {
      risultati[pid] = { errore: { chiave: 'err_scarico', args: [parte.file] } };
      registra(tf('err_scarico', parte.file), true);
    }
    disegnaTabella();
    aggiornaEsito();
  }
  stato(t('pronto'), 0);
}

// ---------- scrittura ----------

async function scrivi() {
  if (inScrittura || !pacchettoVerificato() || !$('conferma').checked) return;
  inScrittura = true;
  $('scrivi').textContent = t('scrivi_in_corso');
  ['pacchetto', 'conferma', 'lingua'].forEach((i) => { $(i).disabled = true; });
  aggiornaEsito();
  // le parti, in ordine di indirizzo
  const parti = pacchettoAttuale.parti
    .map((id) => ({ nome: elenco.parti[id].file.split('/').pop(), indirizzo: elenco.parti[id].indirizzo, byte: risultati[id].byte }))
    .sort((a, b) => a.indirizzo - b.indirizzo);
  try {
    await scriviParti(parti, { registra, stato });
  } catch (e) {
    const testo = e && e.message ? e.message : String(e);
    registra(tf('f_errore', testo), true);
    registra(t('f_errore_nota'), true);
    stato(tf('f_errore', testo));
  } finally {
    inScrittura = false;
    $('scrivi').textContent = t('scrivi');
    ['pacchetto', 'conferma', 'lingua'].forEach((i) => { $(i).disabled = false; });
    $('conferma').checked = false;
    aggiornaEsito();
  }
}

// ---------- avvio ----------

async function avvio() {
  preparaLingua();
  traduci();
  if (!('serial' in navigator)) {
    $('incompatibile').hidden = false;
    $('contenuto').hidden = true;
    $('sottotitolo').hidden = true;
    document.querySelector('footer').hidden = true;
    return;
  }
  try {
    const r = await fetch('firmware.json', { cache: 'no-store', credentials: 'omit' });
    if (!r.ok) throw new Error('http ' + r.status);
    elenco = await r.json();
    if (!elenco || elenco.schema !== 1 || !Array.isArray(elenco.pacchetti) || !elenco.parti) throw new Error('formato');
  } catch (e) {
    $('esito').textContent = t('err_elenco');
    $('esito').className = 'esito ko';
    registra(t('err_elenco'), true);
    return;
  }
  disegnaPacchetti();
  $('pacchetto').addEventListener('change', verificaPacchetto);
  $('conferma').addEventListener('change', aggiornaEsito);
  $('scrivi').addEventListener('click', scrivi);
  stato(t('pronto'), 0);
  await verificaPacchetto();
}

avvio();
