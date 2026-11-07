/* ===== Pin: stato e dati della scheda ===== */
var PM=[],PSEL=-1,PWARN=false,PTM=null,PT={gpio:-1};
var PBI=null,PV="f",PNOTE={};
var PWR={"5V":"#d64545","3V3":"#e08a2c","GND":"#222"};
/* scheda: tutti i numeri arrivano dal firmware (/api/pins/info, file di scheda); senza risposta niente disegno */
function brdLay(k){return PBI&&PBI.lay&&PBI.lay[k]?PBI.lay[k].map(String):[]}
function brdIn(k,g){
  var r=PBI&&PBI[k];
  return !!r&&g>=r[0]&&g<=r[1]
}

function brdMax(){return PBI&&PBI.gmax!=null?PBI.gmax:""}
function brdLoad(){
  return api("/api/pins/info").then(function(r){return r.json()}).then(function(b){
    PBI=b;
    var e=$("io_g");
    if(e)e.max=brdMax();
    return b
  }).catch(function(){})
}
