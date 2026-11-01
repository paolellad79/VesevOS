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
let pacchettoAttuale = null;   // la preimpostazione scelta
let insieme = [];           // id di tutte le parti offerte, in ordine di indirizzo
let scelte = {};            // id parte -> true se la si scrive (le caselle)
let risultati = {};         // id parte -> { r, byte } oppure { errore }
let inScrittura = false;

// ---------- testi e lingua ----------

function traduci() {
  document.querySelectorAll('[data-t]').forEach((e) => { e.textContent = t(e.dataset.t); });
  document.documentElement.lang = $('lingua').value;
  document.title = t('titolo');
  disegnaPacchetti();
  disegnaTabella();
  aggiornaScelte();
}

function preparaLingua() {
  const sel = $('lingua');
  for (const [codice, nome] of Object.entries(LINGUE)) {
    sel.add(new Option(nome, codice));
  }
  sel.value = impostaLingua(linguaIniziale());
  sel.addEventListener('change', () => {
    impostaLingua(sel.value);
    try { localStorage.setItem('vl', sel.value); } catch (e) { /* la scelta vale solo per questa visita */ }
    traduci();
    if (window.SITO) window.SITO.setLang(sel.value);
  });
  if (window.SITO) window.SITO.setLang(sel.value);
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

function azioneTesto(parte, scritta) {
  if (parte.tipo === 'nvs') return t(scritta ? 'az_cancella' : 'az_conserva');
  return t(scritta ? 'az_scrivi' : 'az_lascia');
}

function disegnaTabella() {
  const tab = $('tab-controlli');
  tab.textContent = '';
  if (!elenco) return;
  const capo = tab.createTHead().insertRow();
  for (const c of ['sc_col', 'parte', 'sc_azione', ...COLONNE]) {
    const th = document.createElement('th');
    th.scope = 'col';
    th.textContent = t(c);
    capo.appendChild(th);
  }
  const corpo = tab.createTBody();
  for (const id of insieme) {
    const parte = elenco.parti[id];
    const riga = corpo.insertRow();
    const casella = document.createElement('input');
    casella.type = 'checkbox';
    casella.id = 'sc-' + id;
    casella.className = 'sc';
    casella.checked = !!scelte[id];
    casella.disabled = inScrittura;
    casella.addEventListener('change', () => { scelte[id] = casella.checked; $('conferma').checked = false; aggiornaScelte(); });
    riga.insertCell().appendChild(casella);
    const nome = riga.insertCell();
    const etichetta = document.createElement('label');
    etichetta.htmlFor = casella.id;
    etichetta.textContent = t('tipo_' + parte.tipo) + ' (' + parte.file.split('/').pop() + ')';
    nome.appendChild(etichetta);
    const az = riga.insertCell();
    az.id = 'az-' + id;
    az.textContent = azioneTesto(parte, !!scelte[id]);
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

// ---------- scelta delle parti ----------

function partiScelte() {
  return insieme.filter((id) => scelte[id]);
}

// Avvisi per le combinazioni rischiose: non bloccano, ma si vedono sempre.
function avvisiScelta() {
  const tipi = new Set(partiScelte().map((id) => elenco.parti[id].tipo));
  const chiavi = [];
  if (tipi.has('nvs')) chiavi.push('sc_nvs');
  if (tipi.has('tabella') && !(tipi.has('recovery') && tipi.has('firmware'))) chiavi.push('sc_tabella');
  if (tipi.has('otadata') && !tipi.has('firmware')) chiavi.push('sc_otadata');
  return chiavi;
}

function aggiornaScelte() {
  if (!elenco) return;
  for (const id of insieme) {
    const c = $('sc-' + id), az = $('az-' + id);
    if (c) c.checked = !!scelte[id];
    if (az) az.textContent = azioneTesto(elenco.parti[id], !!scelte[id]);
  }
  const lista = $('avvisi-scelta');
  lista.textContent = '';
  for (const k of avvisiScelta()) { const li = document.createElement('li'); li.textContent = t(k); lista.appendChild(li); }
  aggiornaEsito();
}

function applicaPreimpostazione() {
  pacchettoAttuale = elenco.pacchetti.find((p) => p.id === $('pacchetto').value) || null;
  scelte = {};
  for (const id of insieme) scelte[id] = !!pacchettoAttuale && pacchettoAttuale.parti.includes(id);
  $('conferma').checked = false;
  aggiornaScelte();
}

// ---------- controlli ----------

function primoErrore() {
  for (const id of partiScelte()) {
    const res = risultati[id];
    if (res && res.errore) return res.errore;
    if (res && res.r && res.r.errore) return res.r.errore;
  }
  return null;
}

function pacchettoVerificato() {
  const sel = partiScelte();
  return sel.length > 0 && sel.every((id) => risultati[id] && risultati[id].r && tuttoOk(risultati[id].r));
}

function aggiornaEsito() {
  const e = $('esito');
  e.className = 'esito';
  if (!elenco) { e.textContent = ''; }
  else if (partiScelte().length === 0) { e.textContent = t('sc_nessuna'); e.classList.add('ko'); }
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

async function verificaTutto() {
  risultati = {};
  disegnaTabella();
  aggiornaScelte();
  if (!(window.crypto && crypto.subtle)) {
    risultati[insieme[0]] = { errore: { chiave: 'err_sha_non_disp', args: [] } };
    $('esito').textContent = t('err_sha_non_disp');
    $('esito').className = 'esito ko';
    return;
  }
  for (const pid of insieme) {
    const parte = elenco.parti[pid];
    stato(tf('f_scarico', parte.file), 0);
    try {
      const byte = await scaricaParte(pid, parte);
      const r = await controllaParte(pid, parte, byte);
      risultati[pid] = { r, byte };
      if (!r.errore) registra(tf('f_sha_ok', parte.file));
      else registra(tf(r.errore.chiave, ...r.errore.args), true);
    } catch (e) {
      risultati[pid] = { errore: { chiave: 'err_scarico', args: [parte.file] } };
      registra(tf('err_scarico', parte.file), true);
    }
    disegnaTabella();
    aggiornaScelte();
  }
  stato(t('pronto'), 0);
}

// ---------- scrittura ----------

async function scrivi() {
  if (inScrittura || !pacchettoVerificato() || !$('conferma').checked) return;
  inScrittura = true;
  $('scrivi').textContent = t('scrivi_in_corso');
  ['pacchetto', 'conferma', 'lingua'].forEach((i) => { $(i).disabled = true; });
  document.querySelectorAll('input.sc').forEach((c) => { c.disabled = true; });
  aggiornaEsito();
  // solo le parti scelte, in ordine di indirizzo
  const parti = partiScelte()
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
    document.querySelectorAll('input.sc').forEach((c) => { c.disabled = false; });
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
  insieme = [...new Set(elenco.pacchetti.flatMap((p) => p.parti))]
    .filter((id) => elenco.parti[id]).sort((a, b) => elenco.parti[a].indirizzo - elenco.parti[b].indirizzo);
  disegnaPacchetti();
  $('pacchetto').addEventListener('change', applicaPreimpostazione);
  $('conferma').addEventListener('change', aggiornaEsito);
  $('scrivi').addEventListener('click', scrivi);
  stato(t('pronto'), 0);
  applicaPreimpostazione();
  await verificaTutto();
}

avvio();
