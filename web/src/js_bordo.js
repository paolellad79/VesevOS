/* ===== periferiche di bordo: disegnate dal JSON di /api/dev =====
   Il firmware manda, per ogni periferica gia presente sulla scheda, il campo "wid" (colore, onoff, tasto).
   Qui c'e un disegnatore per ogni tipo di widget: la pagina non conosce ne il chip ne l'elenco dei pezzi. */
/* Voci di menu di bordo: pagina (tab) -> etichetta, per le sole periferiche che la scheda dichiara
   con il widget di quella pagina (campo "bordo" in VOCI). Si aggiorna a ogni lettura di /api/dev. */
var BDV={},BDI={},BDF="";   /* BDV: pagina -> etichetta (alias se c'e); BDI: pagina -> nome vero della periferica */
function bordoVoci(){
  var n={},ids={};
  DV.forEach(function(d){
    var v=d.kind==1&&d.wid?voceDi(d.tab):null;
    if(v&&v.bordo==d.wid&&!n[d.tab]){
      n[d.tab]=etichetta(d.id,d.label);
      ids[d.tab]=d.id
    }
  });
  var a=menuAggiunte();
  var f=JSON.stringify([n,a]);
  BDV=n;
  BDI=ids;
  if(f!=BDF){
    BDF=f;
    menuCostruisci(a);
    subRidisegna()
  }
}

/* Periferiche create a mano (rele, LED, ingressi...): ognuna ha la sua voce nel menu, dopo Periferiche.
   Nomi fatti solo di lettere, cifre, trattino e sottolineatura: il nome entra nell'id della voce. */
function menuAggiunte(){
  return DV.filter(function(d){
    return d.kind==1&&d.tab=="io"&&d.wid===undefined&&/^[A-Za-z0-9_-]+$/.test(d.id)
  }).map(function(d){
    return[d.id,etichetta(d.id,d.label)]
  })
}

function bordoLista(){
  return DV.filter(function(d){
    return d.kind==1&&d.wid!==undefined&&canSee(bordoTab(d))
  })
}

/* Dove porta la riga: ogni periferica ha la sua voce (quelle a mano: "p_" + nome). */
function bordoTab(d){
  return d.wid===undefined&&d.tab=="io"&&POV["p_"+d.id]?"p_"+d.id:d.tab
}

function bordoLoad(){
  dvList().then(bordoDraw).catch(function(){})
}

function bordoDraw(){
  var h="";
  bordoLista().forEach(function(d){
    h+=bordoRiga(d)
  });
  $("bd_card").className=h?"card":"card hide";
  $("bd_list").innerHTML=h?'<ul class="dvl">'+h+'</ul>':""
}

/* Un disegnatore per tipo di widget; se il tipo non si conosce resta la sola riga di stato. */
var BDW={colore:bordoApri,tasto:bordoApri,onoff:bordoOnoff};
function bordoRiga(d){
  var f=BDW[d.wid]||bordoSolo;
  return"<li>"+f(d)+"</li>"
}

/* Colore e tasto: la riga apre la pagina della periferica, dove stanno i suoi controlli. */
function bordoApri(d){
  return'<button class="dvn"'+descrTitle(d.id)+' onclick="show(\''+esc(d.tab)+'\')"><span>'+
    etichettaHtml(d.id,d.label)+'</span>'+dvPill(d)+'</button><span class="dvsw"></span>'
}

function bordoSolo(d){
  return'<span class="dvn" style="cursor:default"'+descrTitle(d.id)+'><span>'+
    etichettaHtml(d.id,d.label)+'</span>'+dvPill(d)+
    '</span><span class="dvsw"></span>'
}

/* Acceso/spento: la riga apre la pagina della periferica; l'interruttore e solo dell'Amministratore, come nell'Elenco dispositivi. */
function bordoOnoff(d){
  var sw="";
  if(d.sw&&ROLE==2)sw=dvSw(d.state==1,etichetta(d.id,d.label),"bordoSet('"+esc(d.id)+"',"+(d.state==1?0:1)+")");
  return'<button class="dvn"'+descrTitle(d.id)+' onclick="show(\''+esc(d.tab)+'\')"><span>'+
    etichettaHtml(d.id,d.label)+'</span>'+dvPill(d)+'</button><span class="dvsw">'+sw+'</span>'
}

function bordoSet(id,on){
  post("/api/dev/set",{id:id,on:on}).then(function(r){return r.json()}).then(function(j){
    msg("bd_m",j.ok?t("Fatto"):j.err,j.ok);
    setTimeout(bordoLoad,300)
  }).catch(function(){})
}
