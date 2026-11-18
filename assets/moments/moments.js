
import * as lib from './lib.js';

const BASE = new URL('./', import.meta.url);
const html = document.documentElement;
const linked = new Map();
function css(href) {
  if (!linked.has(href)) {
    const done = [...document.querySelectorAll('link[rel="stylesheet"]')].some(l => l.href === href);
    linked.set(href, done ? Promise.resolve() : new Promise(res => {
      const l = document.createElement('link');
      l.rel = 'stylesheet';
      l.href = href;
      l.onload = l.onerror = () => res();
      document.head.appendChild(l);
    }));
  }
  return linked.get(href);
}

export function mount(el) {
  if (el.__momentP) return el.__momentP;
  const id = el.dataset.moment;
  if (!id) return Promise.resolve(null);
  el.__momentP = (async () => {
    let opts = {};
    try { opts = JSON.parse(el.dataset.momentOpts || '{}'); } catch { console.warn(`moments: bad data-moment-opts on ${id}`); }
    const card = lib.inCard(el);
    const [, mod] = await Promise.all([css(new URL(`${id}/moment.css`, BASE).href), import(new URL(`${id}/moment.js`, BASE).href)]);
    // a moment light enough for a phone says so in its module (export const phone = true: one picture
    // or one line of words, transform, filter or colour, nothing pinned that a phone cannot hold): it runs
    // there too, and only reduced motion and a card keep it still. Every other moment is still on a phone
    const onPhone = mod.phone === true;
    const why = onPhone ? (lib.reduced() ? 'reduced' : '') : lib.stillWhy();
    if (card) { el.dataset.momentStill = 'card'; html.dataset.momentsCard = [html.dataset.momentsCard, id].filter(Boolean).join(' '); console.warn(`moments: ${id} sits in a card; it stays still`); }
    else if (why) el.dataset.momentStill = why;
    const mlib = { ...lib, reduced: () => card || (onPhone ? lib.reduced() : lib.still()) };
    const api = mod.default(el, opts, mlib) || {};
    el.__moment = api;
    el.dataset.momentMounted = '1';
    return api;
  })();
  return el.__momentP;
}

let docP = null, resolveReady;
export const ready = new Promise(r => { resolveReady = r; });

export function mountAll(root = document) {
  if (root === document && docP) return docP;
  const p = (async () => {
    html.classList.add('mo-js');
    const els = [...root.querySelectorAll('[data-moment]')].filter(el => !el.parentElement?.closest('[data-moment]'));
    const ids = els.map(el => el.dataset.moment);
    const repeat = ids.filter((id, i) => ids.indexOf(id) !== i);
    if (repeat.length) { html.dataset.momentsRepeat = [...new Set(repeat)].join(' '); console.warn(`moments: an id twice on one page: ${repeat.join(', ')}`); }
    if (ids.length > 2) { html.dataset.momentsOver = String(ids.length); console.warn(`moments: ${ids.length} moments on one page (one or two)`); }
    await Promise.all(els.map(el => mount(el).catch(e => {
      if (!el.dataset.momentFailed) console.warn(`moments: ${el.dataset.moment} failed: ${e.message}`);
      el.dataset.momentFailed = e.message;
      el.classList.add('mo-safe');
      el.classList.remove('is-live');   // the hide rules are written under .is-live and ignore .mo-safe: a failed moment shows as written
      el.classList.add('is-done');
    })));
    if (root === document) html.dataset.momentsReady = String(els.length);
    return els;
  })();
  if (root === document) { docP = p; p.then(resolveReady, () => resolveReady([])); }
  return p;
}

export function unmountAll(root = document) {
  root.querySelectorAll('[data-moment-mounted]').forEach(el => { try { el.__moment?.destroy?.(); } catch { /* gone */ } delete el.__moment; delete el.__momentP; el.removeAttribute('data-moment-mounted'); });
  if (root === document) docP = null;
}

html.classList.add('mo-js');
setTimeout(() => html.classList.add('mo-safe'), 2500);
const start = () => { if (!window.__momentsManual) mountAll(document); };
if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', start, { once: true });
else start();

export default { mount, mountAll, unmountAll, ready, lib };
