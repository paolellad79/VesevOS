/* ===== periferiche e Device Manager (API /api/dev) ===== */
var DV=[],DVT={btn:"boot"};
function dvList(){return api("/api/dev").then(function(r){return r.json()}).then(function(j){DV=j.dev||[]})}
function dmLoad(){dvList().then(function(){dvDraw("dm_list","dm_m")}).catch(function(){})}
function dvDraw(box,mm){var L=DV.filter(function(d){return canSee(d.tab)}),caps=[],h="";
 L.forEach(function(d){if(caps.indexOf(d.cap)<0)caps.push(d.cap)});
 if(L.length)h+='<table class="dvt"><colgroup><col class="c1"><col class="c2"><col class="c3"></colgroup>';
 caps.forEach(function(c){h+='<tr class="dvh"><th scope="colgroup" colspan="3">'+esc(c)+'</th></tr>';
  L.filter(function(d){return d.cap==c}).forEach(function(d){
   var st=d.state==1?(d.sw||d.kind==0?t("acceso"):t("presente")):d.state==2?t("con problema"):t("spento");
   var cl=d.state==1?"v":d.state==2?"k":"";
   h+='<tr><td>'+esc(d.label)+'</td><td><span class="pill dvp '+cl+'">'+esc(st)+'</span></td><td><div class="dvact"><span class="dvsw">';
   if(d.sw&&ROLE==2)h+='<button class="sw1'+(d.state?" on":"")+'" role="switch" aria-checked="'+(d.state?"true":"false")+'" aria-label="'+esc(d.label)+'" onclick="dvSet(\''+d.id+'\','+(d.state?0:1)+',\''+box+'\',\''+mm+'\')"></button>';
   h+='</span><button class="btn gray" onclick="show(\''+d.tab+'\')">'+esc(t("Apri"))+'</button></div></td></tr>'})});
 if(L.length)h+='</table>';
 $(box).innerHTML=h||'<p class="fhint">'+esc(t("Nessuna voce"))+'</p>'}
function dvSet(id,on,box,mm){post("/api/dev/set",{id:id,on:on}).then(function(r){return r.json()}).then(function(j){msg(mm,j.ok?t("Fatto"):j.err,j.ok);setTimeout(dmLoad,600)}).catch(function(){})}
function dvRows(tab){var id=DVT[tab]||tab;api("/api/dev?id="+id).then(function(r){return r.json()}).then(function(j){var R=(j.data&&j.data.rows)||[],h='<table>';
 R.forEach(function(x){h+='<tr><td>'+esc(x[0])+'</td><td style="text-align:right"><b>'+esc(x[1])+'</b></td></tr>'});
 $("dv_"+tab).innerHTML=h+'</table>'}).catch(function(){})}

/* ===== periferiche a classi (rele, led, pin, ingresso, analogico): API /api/io, schede modulo /api/mods ===== */
var IOCN={rele:"Rele",led:"LED",pin:"Pin (uscita)",ingresso:"Ingresso",analogico:"Analogico (pin 1-10)"},IOS={sel:"",tm:0,list:[]};
function ioCn(c){return c=="i2c"?t("Sensore I2C (BMP280)"):t(IOCN[c]||c)}
function ioStop(){if(IOS.tm){clearInterval(IOS.tm);IOS.tm=0}}
function ioIco(svg){if(!svg)return'<svg class="i" aria-hidden="true"><use href="#i-chipn"/></svg>';if(svg.indexOf("xmlns")<0)svg=svg.replace("<svg",'<svg xmlns="http://www.w3.org/2000/svg"');return'<img class="api" alt="" width="24" height="24" src="data:image/svg+xml;charset=utf-8,'+enc(svg)+'">'}
function ioFind(n){for(var i=0;i<IOS.list.length;i++)if(IOS.list[i].n==n)return IOS.list[i];return null}
function ioNum(x,dg){return x.toLocaleString(undefined,{minimumFractionDigits:dg,maximumFractionDigits:dg})}
function ioI2cVal(d){return d.ok?ioNum(d.v/100,1)+" °C · "+ioNum(d.p/100,1)+" hPa":t("non risponde")}
function ioPins(d){return d.c=="i2c"?"SDA "+d.g+" · SCL "+d.g2:String(d.g)}
function ioCanSw(d){return d.out&&(ROLE>=2||(ROLE>=1&&d.c!="rele"))}
function ioLoad(){var sel=IOS.sel;$("io_new").className=ROLE>=2?"card":"card hide";$("io_mods").className=ROLE>=2?"card":"card hide";
 api("/api/io").then(function(r){return r.json()}).then(function(j){IOS.list=j.io||[];
  if(sel&&!ioFind(sel)){ioClose();return}
  if(sel){ioDetDraw();return}
  $("io_main").className="";$("io_det").className="card hide";ioStop();ioListDraw();if(ROLE>=2)ioModLoad()}).catch(function(){})}
function ioListDraw(){var L=IOS.list,h="";
 if(!L.length){$("io_tab").innerHTML='<tr><td class="fhint">'+esc(t("Nessuna voce"))+'</td></tr>';return}
 h='<tr><th>'+esc(t("Nome"))+'</th><th>'+esc(t("Classe"))+'</th><th>GPIO</th><th>'+esc(t("Valore"))+'</th><th></th></tr>';
 L.forEach(function(d){var v;
  if(ioCanSw(d))v='<button class="sw1'+(d.v?" on":"")+'" role="switch" aria-checked="'+(d.v?"true":"false")+'" aria-label="'+esc(d.n)+'" onclick="ioSw(\''+d.n+'\','+(d.v?0:1)+')"></button>';
  else if(d.c=="analogico")v='<b>'+d.v+'</b>';
  else if(d.c=="i2c")v='<b>'+esc(ioI2cVal(d))+'</b>';
  else v='<span class="pill dvp '+(d.v?"v":"")+'">'+esc(d.v?t("acceso"):t("spento"))+'</span>';
  h+='<tr><td><b>'+esc(d.n)+'</b></td><td>'+esc(ioCn(d.c))+((d.f&1)?' <span class="fhint">('+esc(t("attivo basso"))+')</span>':"")+'</td><td>'+esc(ioPins(d))+'</td><td>'+v+'</td><td><button class="btn gray" onclick="ioOpen(\''+d.n+'\')">'+esc(t("Gestisci"))+'</button></td></tr>'});
 $("io_tab").innerHTML=h}
/* --- pagina di gestione di una periferica --- */
function ioOpen(n){IOS.sel=n;$("io_main").className="hide";$("io_det").className="card";ioDetDraw();ioStop();IOS.tm=setInterval(ioPoll,1000)}
function ioClose(){IOS.sel="";ioStop();$("io_det").className="card hide";$("io_main").className="";ioLoad()}
function ioPoll(){if(cur!="io"||!IOS.sel){ioStop();return}api("/api/io").then(function(r){return r.json()}).then(function(j){IOS.list=j.io||[];if(IOS.sel&&!ioFind(IOS.sel)){ioClose();return}ioDetLive()}).catch(function(){})}
function ioDetDraw(){var d=ioFind(IOS.sel);if(!d){ioClose();return}var adm=ROLE>=2,sw=ioCanSw(d),h='';
 h+='<div class="aphd"><h3 style="margin:0">'+esc(d.n)+'</h3><button class="btn gray" onclick="ioClose()">'+esc(t("Indietro"))+'</button></div>';
 h+='<table class="ftab" style="margin-top:8px"><tr><td>'+esc(t("Classe"))+'</td><td><b>'+esc(ioCn(d.c))+'</b></td></tr><tr><td>GPIO</td><td><b>'+esc(ioPins(d))+'</b></td></tr>';
 if(d.c=="i2c")h+='<tr><td>'+esc(t("Indirizzo"))+'</td><td><b>0x'+d.a.toString(16)+'</b> · '+esc(d.d)+((d.f&8)?' · 400 kHz':' · 100 kHz')+'</td></tr>';
 if(d.out||d.c=="ingresso")h+='<tr><td>'+esc(t("Livello"))+'</td><td><b>'+esc((d.f&1)?t("attivo basso"):t("attivo alto"))+'</b></td></tr>';
 if(d.c=="ingresso")h+='<tr><td>'+esc(t("Resistenza"))+'</td><td><b>'+esc((d.f&2)?"pull-up":(d.f&4)?"pull-down":t("nessuna"))+'</b></td></tr>';
 h+='<tr><td>'+esc(t("Valore"))+'</td><td id="iod_v"></td></tr></table>';
 h+='<h4 style="margin:14px 0 6px">'+esc(t("Prova"))+'</h4>';
 if(d.out){
  h+='<div class="ftb">';
  if(sw){h+='<button class="btn" onclick="ioAct(\'set\',1)">'+esc(t("Accendi"))+'</button><button class="btn gray" onclick="ioAct(\'set\',0)">'+esc(t("Spegni"))+'</button><button class="btn gray" onclick="ioAct(\'pulse\',1000)">'+esc(t("Impulso di 1 secondo"))+'</button>';
   if(d.c!="rele")h+='<button class="btn gray" onclick="ioAct(\'blink\',5)">'+esc(t("Lampeggia per 5 secondi"))+'</button>'}
  else h+='<span class="fhint">'+esc(t("Il rele lo prova solo l'Amministratore."))+'</span>';
  h+='</div><p class="fhint">'+esc(d.c=="rele"?t("Il rele si riaccende al massimo una volta al secondo e non lampeggia: si consuma. Spegnere si puo sempre."):t("L'impulso e il lampeggio si spengono da soli."))+'</p>'}
 else if(d.c=="i2c")h+='<p class="fhint">'+esc(t("Temperatura e pressione si aggiornano da sole ogni due secondi. Se il sensore non risponde, controlla i fili, l'indirizzo (0x76 o 0x77) e l'alimentazione."))+'</p>';
 else if(d.c=="ingresso")h+='<p class="fhint">'+esc(t("Premi il pulsante o attiva il sensore: il valore qui sotto cambia da solo."))+'</p>';
 else h+='<p class="fhint">'+esc(t("Il valore va da 0 a 4095 e si aggiorna da solo."))+'</p>';
 h+='<div id="iod_m" class="msg" role="status"></div>';
 if(adm)h+='<div class="ftb" style="margin-top:14px"><button class="btn gray" onclick="ioRen(\''+d.n+'\')">'+esc(t("Rinomina"))+'</button><button class="btn red" onclick="ioDel(\''+d.n+'\')">'+esc(t("Togli"))+'</button></div>';
 h+='<p class="fhint">'+esc(d.c=="i2c"?tf("Nelle app: use={0} e poi dev_get(\"{0}\") per la temperatura (centesimi di grado) e dev_get(\"{0}.p\") per la pressione (pascal).",d.n):tf("Nelle app: use={0} e poi dev_set(\"{0}\", 1) o dev_get(\"{0}\").",d.n))+'</p>';
 $("io_det").innerHTML=h;ioDetLive()}
function ioDetLive(){var d=ioFind(IOS.sel),e=$("iod_v");if(!d||!e)return;
 if(d.c=="i2c")e.innerHTML='<b>'+esc(ioI2cVal(d))+'</b>';
 else if(d.c=="analogico")e.innerHTML='<b>'+d.v+'</b> <span class="fhint">/ 4095</span><div class="bar" style="margin-top:4px"><i style="width:'+Math.round(d.v*100/4095)+'%"></i></div>';
 else e.innerHTML='<span class="pill dvp '+(d.v?"v":"")+'">'+esc(d.v?t("acceso"):t("spento"))+'</span>'}
function ioAct(a,v){post("/api/dev/act",{id:IOS.sel,a:a,arg:v}).then(function(r){return r.json()}).then(function(j){if(j.ok===false)msg("iod_m",j.err,false);else msg("iod_m",t("Fatto"),true);ioPoll()}).catch(function(){})}
/* --- schede modulo (.vdp) --- */
function ioModLoad(){api("/api/mods").then(function(r){return r.json()}).then(function(j){var L=j.list||[],h="";
 if(!L.length){$("io_ml").innerHTML='<p class="fhint">'+esc(t("Nessuna scheda modulo. Caricane una con il pulsante qui sotto."))+'</p>';return}
 h='<table class="ftab">';
 L.forEach(function(x){h+='<tr><td><span class="scn">'+ioIco(x.ok?x.icon:"")+'<b>'+esc(x.ok?x.t:x.n)+'</b></span>';
  if(x.ok){h+='<div class="fhint">'+esc(ioCn(x.c))+' x'+x.ns+' · '+esc(x.p=="-"?t("senza alimentazione"):x.p)+(x.l?' · '+esc(x.l):"")+'</div>';if(x.w)h+='<div class="fhint">'+esc(x.w)+'</div>'}
  else h+='<div class="fhint" style="color:var(--kot)">'+esc(x.e||"")+'</div>';
  h+='</td><td style="text-align:right;white-space:nowrap">'+(x.ok?'<button class="btn" onclick="ioModOpen(\''+x.n+'\')">'+esc(t("Installa"))+'</button> ':"")+'<button class="btn gray" onclick="ioModDel(\''+x.n+'\')">'+esc(t("Elimina"))+'</button></td></tr>'});
 $("io_ml").innerHTML=h+'</table>'}).catch(function(){})}
function ioModOpen(n){api("/api/mod?name="+enc(n)).then(function(r){return r.json()}).then(function(j){if(!j.ok){msg("io_mm",j.err,false);return}
  var b='';
  b+=modSchema(j);
  if(j.wiring){var sv=j.wiring;if(sv.indexOf("xmlns")<0)sv=sv.replace("<svg",'<svg xmlns="http://www.w3.org/2000/svg"');b+='<details style="margin-top:8px"><summary>'+esc(t("Disegno allegato al file"))+'</summary><img alt="'+esc(tf("Schema del modulo {0}",j.t))+'" style="width:100%;max-width:420px;background:#fff;border-radius:8px" src="data:image/svg+xml;charset=utf-8,'+enc(sv)+'"></details>'}
  if(j.p=="230V")b+='<p class="fhint" style="color:var(--kot)"><b>'+esc(t("Rete elettrica 230 V: serve un modulo a norma e un installatore. VesevOS comanda solo il segnale."))+'</b></p>';
  if(j.w)b+='<p class="fhint"><b>'+esc(j.w)+'</b></p>';
  if(!j.free)b+='<p class="fhint" style="color:var(--kot)">'+esc(t("Non ci sono abbastanza pin liberi per questo modulo"))+'</p>';
  b+='<label for="io_pf">'+esc(t("Nome (facoltativo)"))+'</label><input id="io_pf" maxlength="9" autocomplete="off" spellcheck="false" autocapitalize="off">';
  dlg({title:j.t,body:b,ok:t("Installa"),onOk:function(){if(!j.free)return t("Non ci sono abbastanza pin liberi per questo modulo");
   return post("/api/mod/install",{name:n,prefix:dlgV("io_pf")}).then(function(r){return r.json()}).then(function(k){if(!k.ok)return k.err;
    var dv=k.devs||[],same=dv.length>1&&dv[0].n==dv[1].n;
    msg("io_mm",t("Fatto")+": "+(same?dv[0].n+" ("+dv.map(function(d){return d.s+" = GPIO"+d.g}).join(", ")+")":dv.map(function(d){return d.n+" = GPIO"+d.g}).join(", ")),true);ioLoad();return""})}})}).catch(function(){})}
function ioModUp(){var f=$("io_mf").files[0];if(!f)return;$("io_mf").value="";var n=f.name.replace(/\.(vdp|txt)$/i,"").toLowerCase();
 if(f.size>8192){msg("io_mm",t("File troppo grande (massimo 8 KB)"),false);return}
 f.text().then(function(x){return post("/api/mod/save",{name:n,text:x})}).then(function(r){return r.json()}).then(function(j){msg("io_mm",j.ok?t("Scheda caricata"):j.err,!!j.ok);if(j.ok)ioModLoad()}).catch(function(){})}
function ioModDel(n){dlg({title:t("Eliminare?"),body:'<p style="margin:0">'+esc(tf("La scheda modulo {0} verra eliminata. Le periferiche gia create restano.",n))+'</p>',ok:t("Elimina"),danger:true,
 onOk:function(){return post("/api/mod/del",{n:n,name:n}).then(function(r){return r.json()}).then(function(j){if(j.ok){ioModLoad();return""}return j.err})}})}
function ioSw(n,v){post("/api/dev/act",{id:n,a:"set",arg:v}).then(function(r){return r.json()}).then(function(j){if(j.ok===false)msg("io_m",j.err,false);ioLoad()}).catch(function(){})}
function ioAdd(){var g=$("io_g").value;post("/api/io/add",{cls:$("io_c").value,gpio:g,name:$("io_n").value,opt:$("io_o").value}).then(function(r){return r.json()}).then(function(j){
 msg("io_am",j.ok?t("Fatto")+": "+j.n:j.err,!!j.ok);if(j.ok){$("io_n").value="";$("io_g").value="";ioLoad()}}).catch(function(){})}
function ioDel(n){dlg({title:t("Togliere la periferica?"),body:'<p style="margin:0">'+esc(n)+'</p>',ok:t("Togli"),danger:true,onOk:function(){return post("/api/io/del",{n:n}).then(function(r){return r.json()}).then(function(j){if(j.ok){ioLoad();return""}return j.err})}})}
function ioRen(n){dlg({title:t("Rinomina"),body:'<label for="dn">'+esc(t("Nuovo nome"))+'</label><input id="dn" autocomplete="off" maxlength="11" value="'+esc(n)+'">',ok:t("Rinomina"),
 onOk:function(){var to=dlgV("dn");return post("/api/io/rename",{n:n,to:to}).then(function(r){return r.json()}).then(function(j){if(j.ok){if(IOS.sel==n)IOS.sel=to;ioLoad();return""}return j.err})}})}

/* ===== dati comuni (paesi, mappa) ===== */
var COMMON=null;
function commonLoad(){if(COMMON)return Promise.resolve(COMMON);return fetch("/api/common").then(function(r){return r.json()}).then(function(j){j.by={};j.countries.forEach(function(c){j.by[c.cc]=c});COMMON=j;return j})}
function cName(c){if(!c)return"";var l=LANG=="it"||c.n[LANG]?LANG:"en";return c.n[l]||c.n.en||c.cc}
function cSorted(){return COMMON.countries.slice().sort(function(a,b){return cName(a).localeCompare(cName(b))})}
function regRule(c){var r=null;if(c&&COMMON)COMMON.regions.forEach(function(x){if(x.id==c.rg)r=x});return r}

function cSelHtml(id,cc,onch){var h='<select id="'+id+'" onchange="'+onch+'"><option value="">'+esc(t("Scegli il paese..."))+'</option>';cSorted().forEach(function(c){h+='<option value="'+c.cc+'"'+(c.cc==cc?" selected":"")+'>'+esc(cName(c))+'</option>'});return h+'</select>'}
function cSearch(q){q=q.trim().toLowerCase();if(!q)return null;var best=null;COMMON.countries.forEach(function(c){var ok=c.cc.toLowerCase()==q;for(var k in c.n)if(c.n[k].toLowerCase().indexOf(q)==0)ok=true;if(ok&&!best)best=c.cc});return best}
/* finestra "Cambia paese" */
var MAPW={cb:null,m:null};
function mapOpen(cb,cc){MAPW.cb=cb;commonLoad().then(function(){var w=$("mapw");
 w.innerHTML='<div class="card" style="width:100%;max-width:900px;margin:0"><h3>'+esc(t("Scegli il paese"))+'</h3><div class="agrid"><div><input id="mw_q" type="search" placeholder="'+esc(t("Cerca (es. Italia, IT)"))+'" oninput="mwFind()"></div><div>'+cSelHtml("mw_s",cc||LOC.cc,"mwSel()")+'</div></div><div class="law">'+esc(t("Il paese decide canali e potenza della radio. Scegli quello dove la scheda si trova davvero."))+'</div><div class="dlga"><button class="btn gray" type="button" onclick="mapClose()">'+esc(t("Annulla"))+'</button><button class="btn" type="button" onclick="mapOk()">'+esc(t("Scegli"))+'</button></div></div>';
 w.className="dlgw";})}
function mwFind(){var c=cSearch($("mw_q").value);if(c)$("mw_s").value=c}
function mwSel(){}
function mapClose(){$("mapw").className="dlgw hide";$("mapw").innerHTML=""}
function mapOk(){var c=$("mw_s").value;if(!c)return;var cb=MAPW.cb;mapClose();if(cb)cb(c)}

/* ===== QR (scritto da noi: modo byte, versioni 1-10) ===== */
var QRE={L:[7,10,15,20,26,18,20,24,30,18],M:[10,16,26,18,24,16,18,22,22,26],Q:[13,22,18,26,18,24,18,22,20,24],H:[17,28,22,16,22,28,26,26,24,28]},QRB={L:[1,1,1,1,1,2,2,2,2,4],M:[1,1,1,2,2,4,4,4,5,5],Q:[1,1,2,2,4,4,6,6,8,8],H:[1,1,2,4,4,4,5,6,8,8]},QRF={L:1,M:0,Q:3,H:2};
function qrRaw(v){var r=(16*v+128)*v+64;if(v>=2){var n=Math.floor(v/7)+2;r-=(25*n-10)*n-55;if(v>=7)r-=36}return r}
function qrDc(v,e){return Math.floor(qrRaw(v)/8)-QRE[e][v-1]*QRB[e][v-1]}
function gfMul(x,y){var z=0;for(var i=7;i>=0;i--){z=(z<<1)^((z>>7)*0x11D);z^=((y>>i)&1)*x}return z&255}
function rsDiv(d){var r=[],i,j,root=1;for(i=0;i<d-1;i++)r.push(0);r.push(1);for(i=0;i<d;i++){for(j=0;j<d;j++){r[j]=gfMul(r[j],root);if(j+1<d)r[j]^=r[j+1]}root=gfMul(root,2)}return r}
function rsRem(data,div){var r=div.map(function(){return 0});data.forEach(function(b){var f=b^r.shift();r.push(0);div.forEach(function(c,i){r[i]^=gfMul(c,f)})});return r}
function qrAlign(v){if(v==1)return[];var n=Math.floor(v/7)+2,st=Math.floor((v*4+n*2+1)/(n*2-2))*2,sz=v*4+17,r=[6];for(var i=0;i<n-1;i++)r.push(sz-7-i*st);return r.sort(function(a,b){return a-b})}
function qrMake(text,ecl){ecl=ecl||"M";var data=utf8(text),v;for(v=1;v<=10;v++)if(4+(v<10?8:16)+data.length*8<=qrDc(v,ecl)*8)break;if(v>10)return null;
 var cap=qrDc(v,ecl)*8,bits=[];function put(val,n){for(var i=n-1;i>=0;i--)bits.push((val>>>i)&1)}
 put(4,4);put(data.length,v<10?8:16);data.forEach(function(b){put(b,8)});put(0,Math.min(4,cap-bits.length));put(0,(8-bits.length%8)%8);for(var pad=0xEC;bits.length<cap;pad^=0xEC^0x11)put(pad,8);
 var cw=[];for(var i=0;i<bits.length;i+=8){var x=0;for(var j=0;j<8;j++)x=x<<1|bits[i+j];cw.push(x)}
 var nb=QRB[ecl][v-1],eb=QRE[ecl][v-1],raw=Math.floor(qrRaw(v)/8),sh=nb-raw%nb,sl=Math.floor(raw/nb),dv=rsDiv(eb),bl=[],k=0;
 for(i=0;i<nb;i++){var dl=sl-eb+(i<sh?0:1),d=cw.slice(k,k+dl);k+=dl;var e=rsRem(d,dv);if(i<sh)d.push(null);bl.push(d.concat(e))}
 var fin=[];for(i=0;i<bl[0].length;i++)bl.forEach(function(b){if(b[i]!==null)fin.push(b[i])});
 var S=v*4+17,m=[],f=[];for(i=0;i<S;i++){m.push(new Array(S).fill(false));f.push(new Array(S).fill(false))}
 function sf(x,y,d){m[y][x]=d;f[y][x]=true}
 for(i=0;i<S;i++){sf(6,i,i%2==0);sf(i,6,i%2==0)}
 function fd(x,y){for(var dy=-4;dy<=4;dy++)for(var dx=-4;dx<=4;dx++){var xx=x+dx,yy=y+dy;if(xx>=0&&xx<S&&yy>=0&&yy<S){var q=Math.max(Math.abs(dx),Math.abs(dy));sf(xx,yy,q!=2&&q!=4)}}}
 fd(3,3);fd(S-4,3);fd(3,S-4);var ap=qrAlign(v);
 ap.forEach(function(a){ap.forEach(function(b){if((a==6&&b==6)||(a==6&&b==S-7)||(a==S-7&&b==6))return;for(var dy=-2;dy<=2;dy++)for(var dx=-2;dx<=2;dx++)sf(a+dx,b+dy,Math.max(Math.abs(dx),Math.abs(dy))!=1)})});
 function fmt(mk){var d=QRF[ecl]<<3|mk,r=d;for(var i=0;i<10;i++)r=(r<<1)^((r>>9)*0x537);var b=(d<<10|r)^0x5412,g=function(i){return((b>>i)&1)!=0};
  for(i=0;i<6;i++)sf(8,i,g(i));sf(8,7,g(6));sf(8,8,g(7));sf(7,8,g(8));for(i=9;i<15;i++)sf(14-i,8,g(i));for(i=0;i<8;i++)sf(S-1-i,8,g(i));for(i=8;i<15;i++)sf(8,S-15+i,g(i));sf(8,S-8,true)}
 fmt(0);if(v>=7){var r=v;for(i=0;i<12;i++)r=(r<<1)^((r>>11)*0x1F25);var b=v<<12|r;for(i=0;i<18;i++){var bt=((b>>i)&1)!=0,a=S-11+i%3,c=Math.floor(i/3);sf(a,c,bt);sf(c,a,bt)}}
 var all=[];fin.forEach(function(c){for(var j=0;j<8;j++)all.push((c>>(7-j))&1)});var n=0;
 for(var rt=S-1;rt>=1;rt-=2){if(rt==6)rt=5;for(var vt=0;vt<S;vt++)for(j=0;j<2;j++){var x=rt-j,upw=((rt+1)&2)==0,y=upw?S-1-vt:vt;if(!f[y][x]&&n<all.length){m[y][x]=all[n]==1;n++}}}
 var MK=[function(x,y){return(x+y)%2==0},function(x,y){return y%2==0},function(x){return x%3==0},function(x,y){return(x+y)%3==0},function(x,y){return(Math.floor(x/3)+Math.floor(y/2))%2==0},function(x,y){return x*y%2+x*y%3==0},function(x,y){return(x*y%2+x*y%3)%2==0},function(x,y){return((x+y)%2+x*y%3)%2==0}];
 function apl(q){for(var y=0;y<S;y++)for(var x=0;x<S;x++)if(!f[y][x]&&MK[q](x,y))m[y][x]=!m[y][x]}
 function pen(){var p=0,cols=[];for(var x=0;x<S;x++){var c=[];for(var y=0;y<S;y++)c.push(m[y][x]);cols.push(c)}
  [m,cols].forEach(function(rows){rows.forEach(function(row){var run=1;for(var a=1;a<S;a++){if(row[a]==row[a-1])run++;else{if(run>=5)p+=run-2;run=1}}if(run>=5)p+=run-2;
   var s=row.map(function(c){return c?"1":"0"}).join("");p+=40*(s.split("10111010000").length-1+s.split("00001011101").length-1)})});
  for(var y=0;y<S-1;y++)for(x=0;x<S-1;x++)if(m[y][x]==m[y][x+1]&&m[y][x]==m[y+1][x]&&m[y][x]==m[y+1][x+1])p+=3;
  var dk=0;m.forEach(function(r){r.forEach(function(c){if(c)dk++})});var tot=S*S;return p+10*(Math.floor((Math.abs(dk*20-tot*10)+tot-1)/tot)-1)}
 var best=-1,bp=0;for(var q=0;q<8;q++){apl(q);fmt(q);var pp=pen();if(best<0||pp<bp){bp=pp;best=q}apl(q)}apl(best);fmt(best);return m}
function qrSvg(text){var m=qrMake(text,"M");if(!m)return"";var n=m.length,d="";for(var y=0;y<n;y++)for(var x=0;x<n;x++)if(m[y][x])d+="M"+(x+2)+" "+(y+2)+"h1v1h-1z";
 return'<svg viewBox="0 0 '+(n+4)+' '+(n+4)+'" shape-rendering="crispEdges" role="img" aria-label="QR"><rect width="100%" height="100%" fill="#fff"/><path d="'+d+'" fill="#000"/></svg>'}
function wifiQr(ss,pw){var e=function(s){return String(s).replace(/([\\;,:"])/g,"\\$1")};return"WIFI:T:WPA;S:"+e(ss)+";P:"+e(pw)+";;"}
/* etichetta da stampare con nome rete, password e QR */
function apLabel(ss,pw){return'<div class="lab prt"><div class="qrbox">'+qrSvg(wifiQr(ss,pw))+'</div><div><div class="subt">'+esc(t("Rete Wi-Fi della scheda"))+'</div><div class="big">'+esc(ss)+'</div><div class="subt" style="margin-top:6px">'+esc(t("Password"))+'</div><div class="pw">'+esc(pw)+'</div><div class="subt" style="margin-top:6px">http://192.168.4.1</div></div></div>'}
var APLAW="La password e casuale e diversa per ogni scheda: una password uguale per tutti aprirebbe tutte le schede, e le leggi sulla sicurezza dei dispositivi lo vietano (UE: direttiva radio con EN 18031 e Cyber Resilience Act; Regno Unito: PSTI). Conservala: stampa l'etichetta o fotografa il QR.";

/* ===== Rete > Punto di accesso ===== */
function drawAp(){var n=RN.n;$("apinfo").innerHTML=row(esc(t("Stato")),esc(n.st==1?t("Attivo"):t("Non attivo (sei collegato a una rete)")))+row(esc(t("Nome rete (SSID)")),esc(n.apSsid))+row(esc(t("Indirizzo IP")),"192.168.4.1")+row(esc(t("Subnet mask")),"255.255.255.0")+macRow(t("MAC punto di accesso"),n.apMac);
 if(!$("ap_ss").value)$("ap_ss").value=n.apSsid||"";if(ROLE>=2)apShow()}
function apShow(){api("/api/ap").then(function(r){return r.json()}).then(function(j){$("apsec").innerHTML=apLabel(j.ssid,j.pass)+'<div class="law">'+esc(t(APLAW))+'</div><button class="btn gray" onclick="print()">'+esc(t("Stampa etichetta"))+'</button>'}).catch(function(){})}
function apSave(nw){var ss=$("ap_ss").value.trim(),pw=$("ap_pw").value;if(!ss){msg("apm",t("Scrivi il nome della rete"),false);return}
 if(pw&&pw.length<8){msg("apm",t("Password Wi-Fi: da 8 a 63 caratteri"),false);return}
 dlg({title:t("Cambiare l'hotspot?"),body:'<p style="margin:0">'+esc(t("Chi e collegato all'hotspot viene scollegato e deve ricollegarsi con i dati nuovi."))+'</p>',ok:t("Cambia"),danger:true,
  onOk:function(){var d={ssid:ss};if(nw)d["new"]="1";else if(pw)d.pass=pw;return post("/api/ap",d).then(function(r){return r.json()}).then(function(j){if(j.ok){$("ap_pw").value="";$("apsec").innerHTML=apLabel(ss,j.pass)+'<div class="law">'+esc(t(APLAW))+'</div>';msg("apm",t("Salvato"),true)}return j.ok?"":j.err})}})}

/* ===== Rete: sottoschede (MQTT spostato in Servizi) ===== */
var RSUB=[["wifi","Wi-Fi"],["ip","Indirizzo IP"],["nome","Nome"],["aereo","Modo aereo"]];
function buildSub(){var h="";RSUB.forEach(function(x){h+='<button class="'+(RN.s==x[0]?"on":"")+'" onclick="rsub(\''+x[0]+'\')">'+esc(t(x[1]))+'</button>'});$("rsub").innerHTML=h;
 RSUB.forEach(function(x){$("r_"+x[0]).className=RN.s==x[0]?"":"hide"})}
function rsub(x){RN.s=x;buildSub();if(x=="aereo")airUi()}

/* ===== MQTT ===== */
function mqLoad(){api("/api/mqtt").then(function(r){return r.json()}).then(function(m){
 svcSt("mqtt",!!m.run);
 $("mqst").innerHTML=(m.run?'<span class="pill'+(m.conn?" a":"")+'">'+esc(m.conn?t("Collegato"):t("non collegato"))+'</span> ':"")+'<span class="fhint">'+esc(tf("Inviati {0}, ricevuti {1}",m.sent,m.recv))+'</span>'+(m.err?'<div class="msg ko" style="display:block;margin-top:6px">'+esc(m.err)+'</div>':"");
 if(document.activeElement&&document.activeElement.closest&&document.activeElement.closest("#t_mqtt .agrid"))return;
 $("mq_host").value=m.host;$("mq_port").value=m.port;$("mq_user").value=m.user;$("mq_pre").value=m.prefix;$("mq_pre").placeholder=m.prefixUsed;$("mq_ev").value=m.every;
 $("mq_pass").placeholder=m.hasPass?t("(salvata: lascia vuoto per non cambiarla)"):"";$("mq_auto").classList.toggle("on",m.auto);$("mq_ha").classList.toggle("on",m.ha);$("mq_tls").classList.toggle("on",!!m.tls);
 $("mqtlsw").className=(!m.tls&&(m.user||m.hasPass))?"law":"law hide";$("mq_ca").placeholder=m.hasCa?t("(certificato salvato: incolla per sostituirlo, salva vuoto per toglierlo)"):"-----BEGIN CERTIFICATE-----";
 var p=m.prefixUsed;$("mqtopics").innerHTML=esc(t("Argomenti:"))+' <code>'+esc(p)+'/state</code> '+esc(t("(stato)"))+', <code>'+esc(p)+'/cmd</code> '+esc(t("(comandi, es. led-color ff0000, gpio 4 1, reboot)"))+', <code>'+esc(p)+'/status</code> (online/offline)'}).catch(function(){})}
function mqSave(){post("/api/mqtt",{host:$("mq_host").value,port:$("mq_port").value,user:$("mq_user").value,pass:$("mq_pass").value,prefix:$("mq_pre").value,every:$("mq_ev").value,auto:$("mq_auto").classList.contains("on")?1:0,ha:$("mq_ha").classList.contains("on")?1:0,tls:$("mq_tls").classList.contains("on")?1:0})
 .then(function(r){return r.json()}).then(function(j){msg("mqm2",j.ok?t("Salvato"):j.err,j.ok);if(j.ok){$("mq_pass").value="";setTimeout(mqLoad,800)}})}
function mqCa(){post("/api/mqtt/ca",{pem:$("mq_ca").value.trim()}).then(function(r){return r.json()}).then(function(j){msg("mqm3",j.ok?t("Salvato"):j.err,j.ok);if(j.ok){$("mq_ca").value="";mqLoad()}})}

/* ===== Rete tra schede ===== */
var MS={d:null,tm:null};
var MROLE=["Nodo","Gateway","Sensore"];
function msLoad(){clearInterval(MS.tm);msPoll();MS.tm=setInterval(function(){if(cur!="mesh"){clearInterval(MS.tm);return}msPoll()},4000);}
function msPoll(){api("/api/mesh").then(function(r){return r.json()}).then(function(m){MS.d=m;
 svcSt("mesh",!!m.run);
 $("msst").innerHTML='<span class="fhint">'+esc(tf("Canale {0} · inviati {1}, ricevuti {2}, scartati {3}",m.ch,m.sent,m.recv,m.bad))+'</span>'+(m.hasKey?"":'<div class="msg ko" style="display:block;margin-top:6px">'+esc(t("Manca la chiave comune: creala in Impostazioni"))+'</div>');
 var foc=document.activeElement&&document.activeElement.closest&&document.activeElement.closest("#t_mesh .card.adm");
 if(!foc&&ROLE==2){$("ms_role").value=m.role;$("ms_auto").classList.toggle("on",m.auto);var h="";for(var c=1;c<=m.chMax;c++)h+='<option value="'+c+'"'+(c==m.ch?" selected":"")+'>'+c+'</option>';$("ms_ch").innerHTML=h}
 var tb='<thead><tr><th>'+esc(t("Nome"))+'</th><th>'+esc(t("Ruolo"))+'</th><th>'+esc(t("Salti"))+'</th><th>'+esc(t("Segnale"))+'</th><th>'+esc(t("Visto"))+'</th><th>'+esc(t("Stato"))+'</th></tr></thead><tbody>';
 if(!m.nodes.length)tb+='<tr><td colspan="6" style="color:var(--mut)">'+esc(t("Nessuna scheda vicina"))+'</td></tr>';
 m.nodes.forEach(function(n){tb+='<tr><td><b>'+esc(n.name||"-")+'</b><br><code class="mac">'+esc(n.mac)+'</code></td><td>'+esc(t(MROLE[n.role]||"?"))+'</td><td>'+n.hops+'</td><td>'+n.rssi+' dBm</td><td>'+esc(durText(n.ago))+'</td><td>'+esc(n.state)+(n.ccx?' <span class="pill k">'+esc(t("altro paese"))+'</span>':"")+'</td></tr>'});
 $("mstab").innerHTML=tb+'</tbody>';
 var to=$("ms_to"),sv=to.value,o='<option value="*">'+esc(t("Tutte"))+'</option>';m.nodes.forEach(function(n){o+='<option value="'+esc(n.mac)+'">'+esc(n.name||n.mac)+'</option>'});to.innerHTML=o;if(sv)to.value=sv;if(!to.value)to.value="*";
 var mh="";m.msgs.forEach(function(x){mh+='<div class="row"><span><b>'+esc(x.from)+'</b> '+esc(x.text)+'</span><span class="fhint">'+esc(x.when)+'</span></div>'});$("msmsg").innerHTML=mh}).catch(function(){})}
function msRun(a){return post("/api/mesh/run",{a:a}).then(function(r){return r.json()}).then(function(j){msg("msm",j.ok?t("Fatto"):j.err,j.ok);setTimeout(msPoll,800)}).catch(function(){msPoll()})}
function msKey(k){if(k==1){post("/api/mesh/key",{}).then(function(r){return r.json()}).then(function(j){$("ms_key").value=j.key||"";if(!j.key)msg("msm2",t("Nessuna chiave salvata"),false)});return}
 dlg({title:t("Creare una chiave nuova?"),body:'<p style="margin:0">'+esc(t("Le altre schede smettono di capire questa finche non copi la chiave nuova anche su di loro."))+'</p>',ok:t("Crea"),danger:true,
  onOk:function(){return post("/api/mesh",{role:$("ms_role").value,auto:$("ms_auto").classList.contains("on")?1:0,newkey:1}).then(function(r){return r.json()}).then(function(j){if(j.ok){$("ms_key").value=j.key;msg("msm2",t("Chiave nuova creata: copiala sulle altre schede"),true)}return j.ok?"":j.err})}})}
function msSave(){var k=$("ms_key").value.trim(),d={role:$("ms_role").value,auto:$("ms_auto").classList.contains("on")?1:0,ch:$("ms_ch").value};if(k&&k!=(MS.key||""))d.key=k;
 post("/api/mesh",d).then(function(r){return r.json()}).then(function(j){msg("msm2",j.ok?t("Salvato"):j.err,j.ok);if(j.ok){MS.key=j.key;msPoll()}})}
function msSend(c){var d={to:$("ms_to").value};if(c){d.cmd=$("ms_cmd").value.trim();if(!d.cmd){msg("msm3",t("Scrivi l'azione"),false);return}}else{d.text=$("ms_tx").value.trim();if(!d.text){msg("msm3",t("Scrivi il messaggio"),false);return}}
 post("/api/mesh/send",d).then(function(r){return r.json()}).then(function(j){msg("msm3",j.ok?t("Inviato"):j.err,j.ok);if(j.ok){$("ms_tx").value="";setTimeout(msPoll,1500)}})}
var BLT=0;
function bleLoad(){api("/api/ble").then(function(r){return r.json()}).then(bleDraw).catch(function(){})}
function bleDraw(b){if(b.have)svcSt("ble",b.on);if(!b.have){$("blecard").innerHTML='<h3>'+esc(t("Bluetooth"))+'</h3><p class="fhint">'+esc(t("Bluetooth non incluso in questo firmware"))+'</p>';return}
 $("blest").innerHTML=b.on?'<div class="law">'+esc(b.lim?tf("Acceso per altri {0}. Nome: {2}. Codice di accoppiamento: {1}",durText(b.left),b.pin,b.name):tf("Acceso, senza limite di tempo. Nome: {1}. Codice di accoppiamento: {0}",b.pin,b.name))+(b.conn?" · "+esc(t("telefono collegato")):"")+'</div>':'<p class="fhint">'+esc(t("Spento"))+(b.last?" · "+esc(b.last):"")+'</p>'}
function bleRun(a){return post("/api/ble",{a:a}).then(function(r){return r.json()}).then(function(j){if(j.ok===false){msg("blem",j.err,false);return}bleDraw(j);msg("blem",t("Fatto"),true)})}

/* ===== Localizzazione ===== */
var LOC={cc:"",r:null,mini:null};
function lcLoad(){Promise.all([api("/api/region").then(function(r){return r.json()}),commonLoad()]).then(function(a){var r=a[0];LOC.r=r;LOC.cc=r.country;fillLangSel();
 $("lc_lang").value=r.lang||LANG;$("lc_ntp").value=r.ntp;$("lc_df").value=r.dateFmt;$("lc_tf").value=r.timeFmt;$("lc_ds").value=r.decSep;$("lc_ws").value=r.weekStart;$("lc_tu").value=r.tempUnit;
 $("lc_ant").value=r.antExt;$("lc_gain").value=r.gain;lcAnt();lcTzFill(r.tz,r.tzName);lcCountry();lcRadio(r)}).catch(function(){})}
function lcCountry(){var c=COMMON.by[LOC.cc];$("lcc").textContent=c?cName(c)+" ("+c.cc+")":t("Paese non scelto: la radio usa le regole piu prudenti (canali 1-11)");
}
function lcTzFill(tz,name){var c=COMMON.by[LOC.cc],L=c?c.tz.slice():[],h="",f=-1;if(!L.some(function(z){return z[0]=="UTC"}))L.push(["UTC","UTC0"]);
 L.forEach(function(z,i){h+='<option value="'+i+'">'+esc(z[0])+'</option>';if(z[1]==tz&&(f<0||z[0]==name))f=i});h+='<option value="-1">'+esc(t("Personalizzato"))+'</option>';
 $("lc_tz").innerHTML=h;$("lc_tz").value=f;LOC.tzl=L;$("lc_posix").value=tz||"";LOC.tzn=name||"";lcTz()}
function lcTz(){var v=+$("lc_tz").value;$("lc_tzc").className=v<0?"":"hide";if(v>=0){$("lc_posix").value=LOC.tzl[v][1];LOC.tzn=LOC.tzl[v][0]}}
function lcAnt(){$("lc_gw").className=$("lc_ant").value=="1"?"":"hide"}
function lcRadio(r){var c=COMMON.by[r.country],g=regRule(c);
 $("lcradio").innerHTML=row(esc(t("Canali")),"1-"+r.ch)+row(esc(t("Limite del paese")),r.limit+" dBm EIRP")+row(esc(t("Potenza massima con questa antenna")),r.max+" dBm")+row(esc(t("Potenza in uso")),r.tx+" dBm")+(g?row(esc(t("Regole")),esc(g.rule)+(g.mark?" · "+esc(g.mark):"")):"");
 var x=$("lc_tx");x.max=r.max;x.value=r.txSet&&r.txSet<=r.max?r.txSet:r.max;lcTx()}
function lcTx(){var x=$("lc_tx");$("lc_txv").textContent=tf("{0} dBm (massimo {1})",x.value,x.max)}
function locPick(cc){LOC.cc=cc;lcDefaults(true)}
function lcDefaults(keep){var c=COMMON.by[LOC.cc];if(!c){msg("lcm",t("Prima scegli il paese"),false);return}
 $("lc_df").value=c.f.d;$("lc_tf").value=c.f.h;$("lc_ds").value=c.f.s;$("lc_ws").value=c.f.w;$("lc_tu").value=c.f.t;$("lc_ntp").value=c.ntp||"pool.ntp.org";
 lcTzFill(c.tz[0][1],c.tz[0][0]);lcCountry();if(LANGS.some(function(l){return l.code==c.f.l}))$("lc_lang").value=c.f.l;
 msg("lcm",t("Valori del paese caricati: premi Salva per applicarli"),true)}
function lcSave(mid){mid=typeof mid=="string"?mid:"lcm";var d={country:LOC.cc,tz:$("lc_posix").value.trim(),tzname:+$("lc_tz").value<0?t("Personalizzato"):LOC.tzn,ntp:$("lc_ntp").value.trim(),datefmt:$("lc_df").value,timefmt:$("lc_tf").value,decsep:$("lc_ds").value,weekstart:$("lc_ws").value,tempunit:$("lc_tu").value,antenna:$("lc_ant").value,gain:$("lc_gain").value||0,txpower:$("lc_tx").value};
 var lg=$("lc_lang").value;
 post("/api/region",d).then(function(r){return r.json()}).then(function(j){if(j.ok===false){msg(mid,j.err,false);return}LOC.r=j;lcRadio(j);msg(mid,t("Salvato"),true);if(lg&&lg!=LANG)setLang(lg)}).catch(function(){})}

/* ===== Watchdog ===== */
var WDD=["Lun","Mar","Mer","Gio","Ven","Sab","Dom"];
function wdLoad(){api("/api/wd").then(function(r){return r.json()}).then(function(w){
 var h=(w.degraded?'<div class="msg ko" style="display:block">'+esc(tf("Modalita ridotta: servizi spenti: {0}. Pagina web, log e recupero restano attivi.",w.off||"-"))+'</div>':"")+(w.stopped?'<div class="msg ko" style="display:block">'+esc(t("Riavvii automatici sospesi: troppi in un'ora. Controlla il registro."))+'</div>':"")+(w.last?row(esc(t("Ultimo riavvio automatico")),esc(w.last)):"");
 h+='<table class="ftab"><thead><tr><th>'+esc(t("Servizio"))+'</th><th>'+esc(t("Ultimo segno di vita"))+'</th><th>'+esc(t("Limite"))+'</th><th>'+esc(t("Riavvii"))+'</th></tr></thead><tbody>';
 w.watch.forEach(function(x){h+='<tr><td>'+esc(x.n)+'</td><td>'+esc(durText(x.ago))+'</td><td>'+esc(durText(x.to))+'</td><td>'+x.r+'</td></tr>'});$("wdst").innerHTML=h+'</tbody></table>';
 $("wd_task").classList.toggle("on",w.task);$("wd_net").classList.toggle("on",w.net);$("wd_ram").classList.toggle("on",w.ram);$("wd_nm").value=w.netMin;$("wd_rk").value=w.ramKb;$("wd_ud").value=w.upDays;
 $("wd_at").value=w.at>=0?("0"+Math.floor(w.at/60)).slice(-2)+":"+("0"+w.at%60).slice(-2):"";
 var b="";WDD.forEach(function(d,i){b+='<button type="button" class="btn '+((w.days>>i)&1?"":"gray")+'" data-on="'+((w.days>>i)&1)+'" onclick="this.dataset.on=this.dataset.on==1?0:1;this.className=\'btn \'+(this.dataset.on==1?\'\':\'gray\')">'+esc(t(d))+'</button>'});$("wd_days").innerHTML=b}).catch(function(){})}
function wdSave(){var at=$("wd_at").value,days=0;[].forEach.call($("wd_days").children,function(b,i){if(b.dataset.on==1)days|=1<<i});
 post("/api/wd",{task:$("wd_task").classList.contains("on")?1:0,net:$("wd_net").classList.contains("on")?1:0,ram:$("wd_ram").classList.contains("on")?1:0,netmin:$("wd_nm").value,ramkb:$("wd_rk").value,updays:$("wd_ud").value,at:at?+at.slice(0,2)*60+ +at.slice(3,5):-1,days:days})
 .then(function(r){return r.json()}).then(function(j){msg("wdm",j.ok?t("Salvato"):j.err,j.ok);if(j.ok)wdLoad()})}

/* ===== Utenti ===== */
function usLoad(){api("/api/users").then(function(r){return r.json()}).then(function(L){var h='<thead><tr><th>'+esc(t("Nome"))+'</th><th>'+esc(t("Ruolo"))+'</th><th>'+esc(t("Attivo"))+'</th><th class="fac"></th></tr></thead><tbody>';
 L.forEach(function(u){h+='<tr><td><b>'+esc(u.name)+'</b>'+(u.name==USER?' <span class="pill">'+esc(t("tu"))+'</span>':"")+(u.pass?"":' <span class="pill k">'+esc(t("senza password"))+'</span>')+'</td><td><select onchange="usSet('+u.i+',\'role\',this.value)">'+[0,1,2].map(function(r){return'<option value="'+r+'"'+(r==u.role?" selected":"")+'>'+esc(t(RNAME[r]))+'</option>'}).join("")+'</select></td>'+
  '<td><button class="sw1'+(u.on?" on":"")+'" onclick="usSet('+u.i+',\'on\','+(u.on?0:1)+')" aria-label="'+esc(t("Attivo"))+'"></button></td><td class="fac"><button class="btn gray sm" onclick="usPass('+u.i+',\''+esc(u.name)+'\')">'+esc(t("Password"))+'</button><button class="btn red sm" onclick="usDel('+u.i+',\''+esc(u.name)+'\')">'+esc(t("Elimina"))+'</button></td></tr>'});
 $("ustab").innerHTML=h+'</tbody>'}).catch(function(){})}
function usSet(i,k,v){var d={i:i};d[k]=v;post("/api/users/set",d).then(function(r){return r.json()}).then(function(j){msg("usm",j.ok?t("Salvato"):j.err,j.ok);usLoad()})}
function usDel(i,n){dlg({title:tf("Eliminare l'utente {0}?",n),body:"",ok:t("Elimina"),danger:true,onOk:function(){return post("/api/users/del",{i:i}).then(function(r){return r.json()}).then(function(j){if(j.ok)usLoad();return j.ok?"":j.err})}})}
function usPass(i,n){dlg({title:tf("Nuova password per {0}",n),body:'<label for="dp1">'+esc(t("Password (min 6)"))+'</label><input type="password" id="dp1" autocomplete="new-password">',ok:t("Salva"),
 onOk:function(){return post("/api/users/pass",{i:i,pass:$("dp1").value}).then(function(r){return r.json()}).then(function(j){if(j.ok){msg("usm",t("Password cambiata"),true);usLoad()}return j.ok?"":j.err})}})}
function uAdd(){post("/api/users/add",{name:$("u_n").value.trim(),role:$("u_r").value,pass:$("u_p").value}).then(function(r){return r.json()}).then(function(j){msg("usm2",j.ok?t("Utente aggiunto"):j.err,j.ok);if(j.ok){$("u_n").value="";$("u_p").value="";usLoad()}})}

/* ===== Filtro IP ===== */
var FW={tm:null,you:""};
function fwLoad(){api("/api/fw").then(function(r){return r.json()}).then(fwDraw).catch(function(){})}
function fwDraw(f){FW.you=f.you;$("fwyou").textContent=tf("Il tuo indirizzo: {0}",f.you)+" · "+(f.youOk?t("con questa regola puoi entrare"):t("ATTENZIONE: con questa regola resteresti fuori"));$("fwyou").style.borderColor=f.youOk?"":"var(--ko)";
 if(document.activeElement!=$("fw_r")){$("fw_m").value=f.mode;$("fw_ntp").classList.toggle("on",f.ntp);$("fw_r").value=f.rules.map(function(x){return(x.from==x.to?x.from:x.from+"-"+x.to)+"|"+(x.on?1:0)+(x.name?"|"+x.name:"")}).join("\n")}
 var h='<thead><tr><th>'+esc(t("Indirizzo"))+'</th><th>'+esc(t("Tentativi"))+'</th></tr></thead><tbody>';if(!f.rejected.length)h+='<tr><td colspan="2" style="color:var(--mut)">'+esc(t("Nessuno"))+'</td></tr>';
 f.rejected.forEach(function(x){h+='<tr><td><code>'+esc(x.ip)+'</code></td><td>'+x.n+'</td></tr>'});$("fwrej").innerHTML=h+'</tbody>';
 clearInterval(FW.tm);$("fwok").className=f["try"]?"btn":"btn hide";
 if(f["try"]){var left=f["try"];var tick=function(){if(cur!="fw"){clearInterval(FW.tm);return}var e=$("fwtry");e.className="msg ko";e.textContent=tf("Regola in prova: se non confermi torna quella di prima tra {0} secondi",left);if(--left<0){clearInterval(FW.tm);fwLoad()}};tick();FW.tm=setInterval(tick,1000)}else{$("fwtry").className="msg";$("fwtry").textContent=""}}
function fwMine(){var v=$("fw_r").value.trim();$("fw_r").value=(v?v+"\n":"")+FW.you+"|1|"+t("io")}
function fwSave(){post("/api/fw",{mode:$("fw_m").value,ntp:$("fw_ntp").classList.contains("on")?1:0,rules:$("fw_r").value}).then(function(r){return r.json()}).then(function(j){if(j.ok===false){msg("fwm",j.err,false);return}fwDraw(j);msg("fwm",t("Regola in prova per 2 minuti: se la pagina risponde ancora premi Conferma"),true)}).catch(function(){msg("fwm",t("Nessuna risposta: aspetta 2 minuti, torna la regola di prima"),false)})}
function fwConfirm(){post("/api/fw/confirm",{}).then(function(r){return r.json()}).then(function(j){msg("fwm",j.ok?t("Regola confermata"):j.err,j.ok);fwLoad()})}

/* ===== HTTPS ===== */
function tlsLoad(){api("/api/tls").then(function(r){return r.json()}).then(tlsDraw).catch(function(){})}
function tlsDraw(x){svcSt("https",x.on,x.pending||!!x.on!=!!AUTH.https,!!AUTH.https);
 if($("certst"))$("certst").innerHTML=row(esc(t("Certificato")),x.custom?esc(t("tuo")):esc(t("creato dalla scheda")))+row(esc(t("Nome")),esc(x.name))+(x.from?row(esc(t("Valido dal")),esc(x.from))+row(esc(t("Scade il")),esc(x.to)):"")+
  '<div class="subt" style="margin-top:6px">'+esc(t("Impronta SHA-256"))+'</div><div class="fp">'+esc(x.fp||"-")+'</div>'+(x.pending?'<div class="law">'+esc(t("Certificato nuovo pronto: vale dal prossimo riavvio."))+'</div>':"");
 $("tlsst").innerHTML=row(esc(t("Stato")),x.on?(AUTH.https?esc(t("Attivo")):esc(t("Attivo dal prossimo riavvio"))):esc(t("Spento")))+row(esc(t("Nome")),esc(x.name))+row(esc(t("Certificato")),x.custom?esc(t("tuo")):esc(t("creato dalla scheda")))+
  '<div class="subt" style="margin-top:6px">'+esc(t("Impronta SHA-256"))+'</div><div class="fp">'+esc(x.fp||"-")+'</div>'+(x.pending?'<div class="law">'+esc(t("Certificato nuovo pronto: vale dal prossimo riavvio."))+'</div>':"")}
function tlsSet(v){if(!v&&!confirm(t("Spegnere HTTPS? Le password restano protette, ma la pagina viaggia in chiaro sulla rete di casa.")))return Promise.resolve();
 return post("/api/tls",{on:v?1:0}).then(function(r){return r.json()}).then(function(j){if(j.ok===false){msg("tlsm",j.err,false);return}tlsDraw(j);svcLoad();msg("tlsm",t("Salvato: vale dopo Applica"),true)})}
function tlsRegen(){dlg({title:t("Creare un certificato nuovo?"),body:'<p style="margin:0">'+esc(t("Vale dal prossimo riavvio. Il browser avvisera di nuovo: confronta l'impronta nuova."))+'</p>',ok:t("Crea"),
 onOk:function(){return post("/api/tls",{regen:1}).then(function(r){return r.json()}).then(function(j){if(j.ok!==false)tlsDraw(j);return j.ok===false?j.err:""})}})}
function tlsUp(){post("/api/tls",{cert:$("tls_c").value.trim(),key:$("tls_k").value.trim()}).then(function(r){return r.json()}).then(function(j){if(j.ok===false){msg("tlsm2",j.err,false);return}$("tls_k").value="";tlsDraw(j);msg("tlsm2",t("Salvato: vale dal prossimo riavvio"),true)})}

