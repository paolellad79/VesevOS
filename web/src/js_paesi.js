/* ===== dati comuni (paesi, mappa) ===== */
var COMMON=null;
function commonLoad(){
  if(COMMON)return Promise.resolve(COMMON);
  return fetch("/api/common").then(function(r){return r.json()}).then(function(j){
    j.by={};
    j.countries.forEach(function(c){j.by[c.cc]=c});
    COMMON=j;
    return j
  })
}

function cName(c){
  if(!c)return"";
  var l=LANG=="it"||c.n[LANG]?LANG:"en";
  return c.n[l]||c.n.en||c.cc
}

function cSorted(){
  return COMMON.countries.slice().sort(function(a,b){return cName(a).localeCompare(cName(b))})
}

function regRule(c){
  var r=null;
  if(c&&COMMON)COMMON.regions.forEach(function(x){if(x.id==c.rg)r=x});
  return r
}

function cSelHtml(id,cc,onch){
  var h='<select id="'+id+'" onchange="'+onch+'"><option value="">'+esc(t("Scegli il paese..."))+
    '</option>';
  cSorted().forEach(function(c){
    h+='<option value="'+c.cc+'"'+(c.cc==cc?" selected":"")+'>'+esc(cName(c))+'</option>'
  });
  return h+'</select>'
}

function cSearch(q){
  q=q.trim().toLowerCase();
  if(!q)return null;
  var best=null;
  COMMON.countries.forEach(function(c){
    var ok=c.cc.toLowerCase()==q;
    for(var k in c.n)if(c.n[k].toLowerCase().indexOf(q)==0)ok=true;
    if(ok&&!best)best=c.cc
  });
  return best
}
/* finestra "Cambia paese" */
var MAPW={cb:null,m:null};
function mapOpen(cb,cc){
  MAPW.cb=cb;
  commonLoad().then(function(){
    var w=$("mapw");
    w.innerHTML='<div class="card" style="width:100%;max-width:900px;margin:0"><h3>'+
      esc(t("Scegli il paese"))+
      '</h3><div class="agrid"><div><input id="mw_q" type="search" placeholder="'+
      esc(t("Cerca (es. Italia, IT)"))+'" oninput="mwFind()"></div><div>'+
      cSelHtml("mw_s",cc||LOC.cc,"mwSel()")+'</div></div><div class="law">'+
      esc(t(
        "Il paese decide canali e potenza della radio. Scegli quello dove la scheda si trova davvero."))+
      '</div><div class="dlga"><button class="btn gray" type="button" onclick="mapClose()">'+
      esc(t("Annulla"))+'</button><button class="btn" type="button" onclick="mapOk()">'+
      esc(t("Scegli"))+'</button></div></div>';
    w.className="dlgw";
  })
}

function mwFind(){
  var c=cSearch($("mw_q").value);
  if(c)$("mw_s").value=c
}

function mwSel(){}
function mapClose(){
  $("mapw").className="dlgw hide";
  $("mapw").innerHTML=""
}

function mapOk(){
  var c=$("mw_s").value;
  if(!c)return;
  var cb=MAPW.cb;
  mapClose();
  if(cb)cb(c)
}
