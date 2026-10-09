/* ===== periferiche e Device Manager (API /api/dev) ===== */
var DV=[],DVT={btn:"boot"};
function dvList(){return api("/api/dev").then(function(r){return r.json()}).then(function(j){DV=j.dev||[]})}
function dmLoad(){dvList().then(function(){dvDraw("dm_list","dm_m")}).catch(function(){})}
function dvDraw(box,mm){var L=DV.filter(function(d){return canSee(d.tab)}),caps=[],h="";
 L.forEach(function(d){if(caps.indexOf(d.cap)<0)caps.push(d.cap)});
 caps.forEach(function(c){h+='<h4 class="gsep" style="margin:12px 0 4px">'+esc(c)+'</h4><table>';
  L.filter(function(d){return d.cap==c}).forEach(function(d){
   var st=d.state==1?(d.sw||d.kind==0?t("acceso"):t("presente")):d.state==2?t("con problema"):t("spento");
   var cl=d.state==1?"v":d.state==2?"k":"";
   h+='<tr><td>'+esc(d.label)+'</td><td><span class="pill '+cl+'">'+esc(st)+'</span></td><td style="text-align:right">';
   if(d.sw&&ROLE==2)h+='<button class="sw1'+(d.state?" on":"")+'" role="switch" aria-checked="'+(d.state?"true":"false")+'" aria-label="'+esc(d.label)+'" onclick="dvSet(\''+d.id+'\','+(d.state?0:1)+',\''+box+'\',\''+mm+'\')"></button> ';
   h+='<button class="btn gray" onclick="show(\''+d.tab+'\')">'+esc(t("Apri"))+'</button></td></tr>'});
  h+='</table>'});
 $(box).innerHTML=h||'<p class="fhint">'+esc(t("Nessuna voce"))+'</p>'}
function dvSet(id,on,box,mm){post("/api/dev/set",{id:id,on:on}).then(function(r){return r.json()}).then(function(j){msg(mm,j.ok?t("Fatto"):j.err,j.ok);setTimeout(dmLoad,600)}).catch(function(){})}
function dvRows(tab){var id=DVT[tab]||tab;api("/api/dev?id="+id).then(function(r){return r.json()}).then(function(j){var R=(j.data&&j.data.rows)||[],h='<table>';
 R.forEach(function(x){h+='<tr><td>'+esc(x[0])+'</td><td style="text-align:right"><b>'+esc(x[1])+'</b></td></tr>'});
 $("dv_"+tab).innerHTML=h+'</table>'}).catch(function(){})}

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

