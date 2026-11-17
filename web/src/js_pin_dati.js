/* ===== Pin: stato e dati della scheda ===== */
var PM=[],PSEL=-1,PWARN=false,PTM=null,PT={gpio:-1};
var PBI=null,PV="f",PNOTE={};
var PWR={"5V":"#d64545","3V3":"#e08a2c","GND":"#222","B+":"#d64545","B-":"#222","BOOST":"#7a6a2a"};
/* scheda: tutti i numeri arrivano dal firmware (/api/pins/info, file di scheda, tabella "tab"); senza risposta niente disegno */
function brdIn(k,g){
  var r=PBI&&PBI[k];
  return !!r&&g>=r[0]&&g<=r[1]
}

function brdMax(){return PBI&&PBI.gmax!=null?PBI.gmax:""}
function brdLoad(){
  return api("/api/pins/info").then(function(r){return r.json()}).then(function(b){
    PBI=b;
    pinTabLoad(b&&b.tab);
    var e=$("io_g");
    if(e)e.max=brdMax();
    return b
  }).catch(function(){})
}
