/* ===== periferica contatore di impulsi (classe "contatore"): voce nell'elenco di aggiunta, dati nel dettaglio e pulsante Azzera; il firmware la offre solo se la include e ci sono unita libere ===== */
function ioCntHz(d){
  var f=d.mhz/1000;
  return f.toFixed(f<10?2:1)+" Hz"
}

function ioCntVal(d){return d.v+" · "+ioCntHz(d)}

function ioCntOpt(free){
  var c=$("io_c"),o=c.querySelector('option[value="contatore"]');
  if(free===undefined)return;                       // firmware senza contatore: nessuna voce
  if(!o){
    o=new Option("","contatore");
    c.add(o)
  }
  o.disabled=free<1;
  o.textContent=free<1?t("Contatore di impulsi (unita finite)"):t("Contatore di impulsi");
  if(o.disabled&&c.value=="contatore"){
    c.value="rele";
    ioClsChg()
  }
}

function ioCntRows(d){
  return '<tr><td>'+esc(t("Fronte da contare"))+'</td><td><b>'+
    esc([t("Salita"),t("Discesa"),t("Salita e discesa")][d.e]||"")+'</b></td></tr><tr><td>'+
    esc(t("Filtro anti-rimbalzo"))+'</td><td><b>'+(d.ns?d.ns+" ns":esc(t("nessuno")))+'</b></td></tr>'
}

function ioCntDet(d){
  var h='';
  if(ROLE>=1)h+='<div class="ftb"><button class="btn gray" onclick="ioAct(\'reset\',0)">'+
    esc(t("Azzera"))+'</button></div>';
  return h+'<p class="fhint">'+esc(tf(
    "Dopo il riavvio il contatore riparte da 0. Nelle app: use={0} e poi contatore(\"{0}\").",d.n))+'</p>'
}

function ioCntOpts(){
  var r=$("io_cr").value;
  return "fronte="+$("io_cf").value+",filtro="+($("io_cn").value||0)+(r?","+r:"")
}
