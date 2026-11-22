// Prova di parita, lato pagina: legge tools/hosttest/casi_formato_data.txt e controlla web/src/js_dataora.js.
// Gli stessi casi li legge prova_formato_data.cpp per il firmware (core/vos_datefmt.cpp).
// Uso: node tools/hosttest/prova_formato_data.js
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const repo = path.join(__dirname, '..', '..');
const codice = fs.readFileSync(path.join(repo, 'web', 'src', 'js_dataora.js'), 'utf8');
const casi = fs.readFileSync(path.join(__dirname, 'casi_formato_data.txt'), 'utf8').split('\n');

// Nomi come li costruisce la pagina: t() restituisce il testo della frase, qui una tabella cambiata a ogni riga N*
const nomiProva = { NB: [], NL: [], NM: [], NF: [] };
const ctx = { DICT: {}, HW: null, AUTH: null };
ctx.t = function (s) {
  const g = ['Dom.', 'Lun.', 'Mar.', 'Mer.', 'Giov.', 'Ven.', 'Sab.'];
  const gl = ['Domenica', 'Lunedi', 'Martedi', 'Mercoledi', 'Giovedi', 'Venerdi', 'Sabato'];
  const ml = ['gennaio', 'febbraio', 'marzo', 'aprile', 'maggio', 'giugno', 'luglio', 'agosto', 'settembre', 'ottobre', 'novembre', 'dicembre'];
  const mf = ['Gen.', 'Feb.', 'Mar.', 'Apr.', 'Mag.', 'Giu.', 'Lug.', 'Ago.', 'Set.', 'Ott.', 'Nov.', 'Dic.'];
  let i = g.indexOf(s);
  if (i >= 0 && nomiProva.NB.length) return nomiProva.NB[i];
  i = gl.indexOf(s);
  if (i >= 0 && nomiProva.NL.length) return nomiProva.NL[i];
  i = ml.indexOf(s);
  if (i >= 0 && nomiProva.NM.length) return nomiProva.NM[i];
  const m = /^\{0\} (\w+\.) \{1\}$/.exec(s);
  if (m && nomiProva.NF.length) {
    i = mf.indexOf(m[1]);
    if (i >= 0) return nomiProva.NF[i].replace('%d', '{0}').replace('%d', '{1}');
  }
  return s;
};
vm.createContext(ctx);
vm.runInContext(codice, ctx);

function parti(testo) {
  const n = testo.split(' ').map(Number);
  return { anno: n[0], mese: n[1], giorno: n[2], giornoSett: n[3], ore: n[4], minuti: n[5], secondi: n[6] };
}

let prove = 0;
let fallite = 0;
casi.forEach(function (riga, k) {
  if (!riga || riga[0] === '#') return;
  const c = riga.split('\t');
  if (c.length !== 5) {
    console.log('  KO riga ' + (k + 1) + ': servono 5 campi');
    fallite++;
    return;
  }
  const tipo = c[0];
  if (tipo === 'NB' || tipo === 'NL' || tipo === 'NM') {
    nomiProva[tipo] = c[1].split(',');
    ctx.DICT = {};          // dizionario nuovo: la pagina rifa i nomi
    return;
  }
  if (tipo === 'NF') {
    nomiProva.NF = c[1].split(';');
    ctx.DICT = {};
    return;
  }
  let ottenuto;
  if (tipo === 'M') {
    ottenuto = ctx.dfMeseBreve(c[1]);
  } else {
    const d = parti(c[3]);
    const nomi = vm.runInContext('dfNomi()', ctx);
    const im = { dataCorta: 0, dataCortaTesto: '', dataLunga: 0, dataLungaTesto: '', ampm: false, oraLungaSec: false, oraLunga: 0, oraLungaTesto: '' };
    if (tipo === 'F') {
      const e = ctx.dfControlla(c[1]);
      ottenuto = e ? '!' + e : ctx.dfFormatta(c[1], d, nomi);
    } else if (tipo === 'DC') {
      im.dataCorta = parseInt(c[1], 10);
      im.dataCortaTesto = c[2];
      ottenuto = ctx.dfDataCorta(im, d, nomi);
    } else if (tipo === 'DL') {
      im.dataLunga = parseInt(c[1], 10);
      im.dataLungaTesto = c[2];
      ottenuto = ctx.dfDataLunga(im, d, nomi);
    } else if (tipo === 'OC') {
      im.ampm = parseInt(c[1], 10) !== 0;
      ottenuto = ctx.dfOraCorta(im, d);
    } else if (tipo === 'OL') {
      const s = c[1].split(' ').map(Number);
      im.ampm = s[0] !== 0;
      im.oraLungaSec = s[1] !== 0;
      im.oraLunga = s[2];
      im.oraLungaTesto = c[2];
      ottenuto = ctx.dfOraLunga(im, d, nomi);
    } else {
      console.log('  KO riga ' + (k + 1) + ': tipo sconosciuto ' + tipo);
      fallite++;
      return;
    }
  }
  prove++;
  if (ottenuto !== c[4]) {
    fallite++;
    console.log('  KO riga ' + (k + 1) + ' (' + tipo + ' ' + c[1] + '): atteso [' + c[4] + '], ottenuto [' + ottenuto + ']');
  }
});
console.log('formato data (pagina): ' + prove + ' prove, ' + fallite + ' fallite');
process.exit(fallite ? 1 : 0);
