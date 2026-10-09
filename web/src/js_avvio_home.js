/* ===== Ordine di avvio ===== */
var BT={d:null,roots:[],ch:{}};
var BN=[["sys","Sistema e monitor"],["led","LED"],["net","Rete"],["time","Ora"],["web","Pagina web e API"],["rules","Automazioni"],["mqtt","MQTT"],["mesh","Rete tra schede"],["wd","Watchdog"]];
var BFIX=[["log","Registro (log)"],["pin","Pin e risorse"],["fs","File e configurazione"],["lang","Lingua"]];
function bn(id){return lk(BN,id)}
function bLoad(){api("/api/boot").then(function(r){return r.json()}).then(function(d){BT.d=d;var pos={};d.order.forEach(function(x,i){pos[x]=i});
  BT.roots=[];BT.ch={};d.svc.forEach(function(x){if(!x.parent)BT.roots.push(x.id);else{(BT.ch[x.parent]=BT.ch[x.parent]||[]).push(x.id)}});
  BT.roots.sort(function(a,b){return pos[a]-pos[b]});for(var k in BT.ch)BT.ch[k].sort(function(a,b){return pos[a]-pos[b]});bDraw()}).catch(function(){})}
function bSvc(id){return BT.d.svc.filter(function(x){return x.id==id})[0]}
function bRow(id,child){var x=bSvc(id),rq=x.req.length?tf("Richiede: {0}",x.req.map(function(q){return bn(q)}).join(", ")):t("Non richiede altri servizi");
 var tm=tf("partito a +{0} ms, ha impiegato {1} ms",x.at,x.ms);
 return'<div class="brow"><span class="grip" data-g="'+id+'"><svg class="i"><use href="#i-grip"/></svg></span><div class="bn"><b>'+esc(bn(id))+'</b><small>'+esc(rq)+'</small><small>'+esc(tm)+'</small></div>'+
 '<button class="bi" onclick="bMove(\''+id+'\',-1)" aria-label="'+esc(t("Su"))+'">&uarr;</button><button class="bi" onclick="bMove(\''+id+'\',1)" aria-label="'+esc(t("Giu"))+'">&darr;</button></div>'}
function bNode(id,lv){var h='<div class="'+(lv?'bc':'bg')+'" data-id="'+id+'">'+bRow(id)+'<div class="bch">';(BT.ch[id]||[]).forEach(function(c){h+=bNode(c,lv+1)});return h+'</div></div>'}
function bDraw(){var f="";BFIX.forEach(function(x){f+='<div class="bfix"><svg class="i"><use href="#i-lock"/></svg>'+esc(t(x[1]))+'</div>'});$("bfix").innerHTML=f;
 var h="";BT.roots.forEach(function(r){h+=bNode(r,0)});$("bl").innerHTML=h;
 var w=$("bwarn");if(BT.d.fell){w.className="msg ko";w.textContent=t("L'ordine scelto non ha funzionato: e stato ripristinato quello predefinito.")}else if(BT.d.custom){w.className="msg ok";w.textContent=t("Ordine personalizzato attivo.")}else{w.className="msg";w.textContent=""}
 bGrips()}
function bFlat(){var o=[];function w(id){o.push(id);(BT.ch[id]||[]).forEach(w)}BT.roots.forEach(w);return o}
function bRead(box,pid){var ids=[].filter.call(box.children,function(c){return c.hasAttribute("data-id")}).map(function(c){return c.getAttribute("data-id")});
 if(pid)BT.ch[pid]=ids;else BT.roots=ids;ids.forEach(function(id){bRead(box.querySelector('[data-id="'+id+'"]>.bch'),id)})}
function bMove(id,d){var a=BT.roots.indexOf(id)>=0?BT.roots:BT.ch[bSvc(id).parent];var i=a.indexOf(id),j=i+d;if(j<0||j>=a.length)return;var x=a[i];a[i]=a[j];a[j]=x;bDraw()}
function bGrips(){var gs=$("bl").querySelectorAll(".grip");
 for(var i=0;i<gs.length;i++)(function(g){
  g.addEventListener("pointerdown",function(e){var el=g.closest(".bc,.bg");el.classList.add("drag");e.preventDefault();
   function mv(ev){if(ev.clientY>innerHeight-70)scrollBy(0,18);else if(ev.clientY<130)scrollBy(0,-18);var par=el.parentNode,sib=[].filter.call(par.children,function(c){return c!==el&&c.hasAttribute("data-id")}),ins=null;
    for(var k=0;k<sib.length;k++){var r=sib[k].getBoundingClientRect();if(ev.clientY<r.top+r.height/2){ins=sib[k];break}}
    if(ins)par.insertBefore(el,ins);else par.appendChild(el)}
   function up(){document.removeEventListener("pointermove",mv);document.removeEventListener("pointerup",up);document.removeEventListener("pointercancel",up);el.classList.remove("drag");bRead($("bl"),null);bDraw()}
   document.addEventListener("pointermove",mv);document.addEventListener("pointerup",up);document.addEventListener("pointercancel",up)})})(gs[i])}
function bSave(){post("/api/boot",{order:bFlat().join(",")}).then(function(r){return r.json()}).then(function(j){if(j.ok){msg("bm",t("Salvato: vale dal prossimo riavvio"),true);bLoad()}else msg("bm",j.err||t("Errore"),false)})}
function bReset(){post("/api/boot/reset",{}).then(function(){msg("bm",t("Ordine predefinito (vale dal prossimo riavvio)"),true);bLoad()})}
function bReboot(){if(!confirm(t("Riavviare la scheda?")))return;post("/api/shell",{c:"reboot"}).catch(function(){});msg("bm",t("Riavvio..."),true);setTimeout(function(){location.reload()},9000)}
var AUTHSET=true;
function uMenu(e){e.stopPropagation();var m=$("um"),open=m.classList.contains("hide");$("thm").className="pop hide";m.className=open?"pop":"pop hide";if(open){var b=m.querySelector("button");if(b)b.focus()}}
function umClose(){$("um").className="pop hide"}
document.addEventListener("click",function(e){var m=$("um");if(m&&!m.classList.contains("hide")&&!e.target.closest("#um"))umClose()});
function uWait(txt,ms){dlg({title:txt,body:'<p style="margin:0">'+esc(t("La pagina si ricarica da sola quando la scheda e pronta."))+'</p>',buttons:[]});setTimeout(function(){location.reload()},ms)}
function uReboot(){dlg({title:t("Riavviare la scheda?"),body:'<p style="margin:0">'+esc(t("La pagina resta irraggiungibile per qualche secondo."))+'</p>',ok:t("Riavvia"),onOk:function(){return post("/api/reboot",{}).then(function(r){return r.json()}).then(function(j){if(j.ok)setTimeout(function(){uWait(t("Riavvio in corso..."),10000)},50);return j.ok?"":j.err})}})}
function uSleep(){dlg({title:t("Mettere la scheda in sleep?"),body:'<p style="margin:0">'+esc(t("La scheda entra in sonno profondo: Wi-Fi e LED spenti, consuma pochissimo."))+'</p><p style="margin:8px 0 0"><b>'+esc(t("Per riaccenderla premi il tasto RESET sulla scheda (o togli e ridai corrente)."))+'</b></p>',ok:t("Sleep"),danger:true,
 onOk:function(){return post("/api/sleep",{}).then(function(r){return r.json()}).then(function(j){if(j.ok)setTimeout(function(){dlg({title:t("Scheda in sleep"),body:'<p style="margin:0">'+esc(t("Premi RESET sulla scheda, poi ricarica questa pagina."))+'</p>',buttons:[["ok",t("Ricarica"),""]],onOk:function(){location.reload();return""}})},50);return j.ok?"":j.err})}})}
function row(a,b){return '<div class="row"><span>'+a+'</span><span>'+b+'</span></div>'}
function macRow(lab,m){if(!m)return"";return row(esc(lab),'<code class="mac">'+esc(m)+'</code>')}
function bar(p){return '<div class="bar"><i style="width:'+p+'%"></i></div>'}
function tmp(c,u){return u?(c*9/5+32).toFixed(1)+" &deg;F":c.toFixed(1)+" &deg;C"}
function kb(n){return Math.round(n/1024)+" KB"}
var PBUSY=0;document.addEventListener("visibilitychange",function(){if(!document.hidden&&typeof loggedIn=="function"&&loggedIn())poll()});
var RF=3;try{var q=parseInt(localStorage.getItem("vosRf"));if([0,1,3,5,10,30].indexOf(q)>=0)RF=q}catch(e){}
function rfApply(){clearInterval(timer);timer=null;if($("rf"))$("rf").value=String(RF);if(RF>0)timer=setInterval(poll,RF*1000)}
function rfSet(v){RF=parseInt(v);try{localStorage.setItem("vosRf",String(RF))}catch(e){}rfApply();if(RF>0)poll()}
function poll(){if(PBUSY&&Date.now()-PBUSY<8000)return;if(document.hidden)return;PBUSY=Date.now();api("/api/status").then(function(r){return r.json()}).then(function(s){S=s;if($("fver"))$("fver").textContent="VesevOS "+s.version+" \u00b7 (C) 2026 Domenico Paolella";
 hdTick();hwFetch();$("bchip").textContent=s.net.host+" · "+boardId(s);
 var hp=Math.round(100-100*s.heapFree/s.heapTotal),pp=s.psramTotal?Math.round(100-100*s.psramFree/s.psramTotal):0,fp=s.flashTotal?Math.round(100*s.flashUsed/s.flashTotal):0;
 if($("cpumode")&&document.activeElement!=$("cpumode"))$("cpumode").value=s.cpuMode;
 alertsCalc(s,hp,pp,fp);if(cur=="sum"){homeDraw()}PBUSY=0}).catch(function(){PBUSY=0})}
function boardId(s){var m=(s.net.mac||"").split(":");return m.length==6?(m[4]+m[5]).toUpperCase():"----"}
function hcol(p,w,k){return p>=k?"var(--ko)":p>=w?"var(--wa)":"var(--ok)"}
function hring(l,txt,p,c,icon,go,sub,ex){var R=38,C=2*Math.PI*R;return'<div class="ring" onclick="'+"show('"+go+"')"+'"><svg viewBox="0 0 100 100"><circle cx="50" cy="50" r="'+R+'" fill="none" stroke="var(--trk)" stroke-width="9"/><circle cx="50" cy="50" r="'+R+'" fill="none" stroke="'+c+'" stroke-width="9" stroke-linecap="round" stroke-dasharray="'+(C*Math.max(0,Math.min(100,p))/100)+' '+C+'" transform="rotate(-90 50 50)"/><g fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" style="color:var(--mut)"><use href="#'+icon+'" x="35" y="35" width="30" height="30"/></g></svg><div class="v">'+txt+'</div><div class="l">'+esc(l)+'</div>'+(sub?'<div class="l">'+sub+'</div>':'')+(ex?'<div class="rx">'+ex+'</div>':'')+'</div>'}
function hbar(l,v,p,w,k){var c=p>=k?" k":p>=w?" w":"";return'<div class="pb'+c+'"><div class="t"><span>'+esc(l)+'</span><span>'+v+'</span></div><div class="tr"><i style="width:'+Math.min(100,p)+'%"></i></div></div>'}
function harea(a,mn,mx,color,id){var w=600,h=120,n=a.length;if(n<2)return"";if(mn===null){mn=Math.min.apply(null,a)-2;mx=Math.max.apply(null,a)+2}
 var pts=a.map(function(v,i){return[i*w/59,h-6-((v-mn)/(mx-mn))*(h-14)]}),l=pts.map(function(q,i){return(i?"L":"M")+q[0].toFixed(1)+" "+q[1].toFixed(1)}).join(" "),g="";
 for(var i=1;i<4;i++)g+='<line x1="0" x2="'+w+'" y1="'+i*h/4+'" y2="'+i*h/4+'" stroke="var(--bd)"/>';
 return'<svg viewBox="0 0 '+w+' '+h+'" preserveAspectRatio="none" style="width:100%;height:110px;display:block"><defs><linearGradient id="'+id+'" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="'+color+'" stop-opacity=".35"/><stop offset="1" stop-color="'+color+'" stop-opacity="0"/></linearGradient></defs>'+g+'<path d="'+l+" L"+(pts[n-1][0]).toFixed(1)+" "+h+" L0 "+h+'Z" fill="url(#'+id+')"/><path d="'+l+'" fill="none" stroke="'+color+'" stroke-width="2.5" vector-effect="non-scaling-stroke" stroke-linejoin="round"/></svg>'}
var AL=[];
function alertsCalc(s,hp,pp,fp){AL=[];var n=s.net;if(hp===undefined){hp=Math.round(100-100*s.heapFree/s.heapTotal);fp=s.flashTotal?Math.round(100*s.flashUsed/s.flashTotal):0}
 (typeof AU!="undefined"?AU.al:[]).forEach(function(a){if(!a.ack)AL.push([a.lv==2?"k":"w",a.text,1])});
 if(s.hot)AL.push(["k",t("Temperatura CPU troppo alta")]);
 if(hp>=90)AL.push(["k",t("RAM quasi esaurita")]);else if(hp>=80)AL.push(["w",t("RAM molto usata")]);
 if(fp>=90)AL.push(["w",t("Spazio file quasi pieno")]);
 if(n.st==3&&n.rssi<-75)AL.push(["w",t("Segnale Wi-Fi debole")]);
 if(n.st==1&&n.staFail)AL.push(["w",t("La scheda non e riuscita a collegarsi alla Wi-Fi di casa: controlla nome e password in Rete > Wi-Fi.")]);
 if(n.st==1)AL.push(["i",t("Modo punto di accesso: la scheda non e collegata a una rete")]);
 if(n.st==2)AL.push(["w",t("Collegamento Wi-Fi in corso")]);
 var b=$("bdg"),k=AL.filter(function(x){return x[0]!="i"}).length;b.textContent=AL.length;b.className="bdg"+(AL.some(function(x){return x[0]=="k"})?" k":"")+(AL.length?"":" hide");void k;
 if(!$("alerts").classList.contains("hide"))bellDraw()}
function bellDraw(){var h="";if(!AL.length)h="<div style='color:var(--mut)'>"+esc(t("Nessun allarme"))+"</div>";AL.forEach(function(a){h+="<div"+(a[2]?" style='cursor:pointer' onclick=\"show('audit')\"":"")+"><span class='dot' style='background:"+(a[0]=="k"?"var(--ko)":a[0]=="w"?"var(--wa)":"var(--ac)")+"'></span>"+esc(a[1])+"</div>"});$("alerts").innerHTML=h}
function bellTog(){var e=$("alerts");if(e.classList.contains("hide")){bellDraw();e.className="alerts"}else e.className="alerts hide"}
function hlines(L,mn,mx,px){var w=600,h=100,g="";for(var i=1;i<4;i++)g+='<line x1="0" x2="'+w+'" y1="'+i*h/4+'" y2="'+i*h/4+'" stroke="var(--bd)"/>';
 var p=L.map(function(q){var a=q[0]||[];if(a.length<2)return"";var d=a.map(function(v,i){return(i?"L":"M")+(i*w/59).toFixed(1)+" "+(h-4-((v-mn)/(mx-mn))*(h-8)).toFixed(1)}).join(" ");return'<path d="'+d+'" fill="none" stroke="'+q[1]+'" stroke-width="2" vector-effect="non-scaling-stroke"/>'}).join("");
 return'<svg viewBox="0 0 '+w+' '+h+'" preserveAspectRatio="none" style="width:100%;height:'+(px||90)+'px;display:block">'+g+p+'</svg>'}
function hleg(L){return'<div class="leg">'+L.map(function(q){return'<span><i style="background:'+q[1]+'"></i>'+esc(q[0])+' '+q[2]+'</span>'}).join("")+'</div>'}
function bps(v){return v>=1024?(v/1024).toFixed(1)+' KB/s':v+' B/s'}
function memTx(u,t0){return kb(u)+' / '+kb(t0)+'<br>'+kb(t0-u)+' '+esc(t("liberi"))}
function homeDraw(){var s=S;if(!s||!$("home"))return;var n=s.net,hp=Math.round(100-100*s.heapFree/s.heapTotal),pp=s.psramTotal?Math.round(100-100*s.psramFree/s.psramTotal):0,fp=s.flashTotal?Math.round(100*s.flashUsed/s.flashTotal):0;
 var wp=n.st==3?2*(n.rssi+100):100,wt=n.st==3?n.rssi+" dBm":(n.st==1?""+(n.clients||0):n.st==4?t("Aereo"):"..."),wc=n.st==3?(n.rssi>-67?"var(--ok)":n.rssi>-75?"var(--wa)":"var(--ko)"):"var(--ac)",
 ws=esc(n.mode)+(n.st==1?" · "+esc(tf("{0} client",n.clients||0)):"")+(n.ip?"<br>"+esc(n.ip):"");
 var rv=s.rev,revs=Math.floor(rv/100)+"."+rv%100,mhz=s.cpuMhz;
 var th=(s.tempHist||[]).map(function(v){return s.tempUnit?v*9/5+32:v});
 var cx=hlines([[s.cpu0Hist,"#3fa7ff"],[s.cpu1Hist,"#b07cff"]],0,100,34)+hleg([["C0","#3fa7ff",s.cpu0+"%"],["C1","#b07cff",s.cpu1+"%"]])+hlines([[th,"#ff9f43"]],th.length?Math.min.apply(null,th)-2:0,th.length?Math.max.apply(null,th)+2:1,26)+hleg([[t("Temp."),"#ff9f43",tmp(s.temp,s.tempUnit)]]);
 var ih=s.inHist||[],oh=s.outHist||[],mx=Math.max(1024,Math.max.apply(null,ih.concat(oh,[0]))),wx=hlines([[ih,"#3fa7ff"],[oh,"#ff9f43"]],0,mx,34)+hleg([["In","#3fa7ff",bps(ih.length?ih[ih.length-1]:0)],["Out","#ff9f43",bps(oh.length?oh[oh.length-1]:0)]]);
 var h='<section class="rings">'+hring("CPU",s.cpu+"%",s.cpu,hcol(s.cpu,60,85),"i-chipn","task",mhz+" MHz",cx)+hring("RAM",hp+"%",hp,hcol(hp,80,92),"i-mem","task",memTx(s.heapTotal-s.heapFree,s.heapTotal),hlines([[s.ramHist,"#3fa7ff"]],0,100,34))+hring("PSRAM",pp+"%",pp,hcol(pp,80,92),"i-mem","task",memTx(s.psramTotal-s.psramFree,s.psramTotal),hlines([[s.psHist,"#2ecc71"]],0,100,34))+hring(t("File"),fp+"%",fp,hcol(fp,75,90),"i-disk","file",memTx(s.flashUsed,s.flashTotal),hlines([[s.flHist,"#ffb020"]],0,100,34))+hring(t("Wi-Fi"),wt,wp,wc,"i-wifi","wifi",ws,wx)+'</section>';
 h+=homeWid(s,n);
 h+='<section class="card hid"><h3>'+esc(t("Sistema"))+'</h3><div class="big">'+esc(n.host)+'</div><div class="subt">'+esc(s.chip)+" · ID "+boardId(s)+'</div><div style="height:8px"></div>'+
  row(t("Chip"),esc(s.chip)+" rev "+revs)+row(t("Processore"),tf("{0} core a {1} MHz",s.cores,mhz))+row(t("Memoria"),tf("{0} MB flash · {1} MB PSRAM",Math.round(s.flashChip/1048576),Math.round(s.psramTotal/1048576)))+
  row(t("Acceso da"),esc(s.uptime))+row(t("Ore di vita"),tf("{0} h {1} min",Math.floor(s.lifeSec/3600),Math.floor(s.lifeSec/60)%60))+row(t("Avvii totali"),s.boots)+row(t("Ultimo reset"),esc(s.reset))+row(t("Versione"),esc(s.name+" "+s.version))+
  '<button class="btn" style="margin-top:10px" onclick="identify()">'+esc(t("Trova questa scheda"))+'</button><div id="idm" class="msg"></div></section>';
$("home").innerHTML=h}
var HW=null,HWT=0;
function hdTick(){var e=$("hd");if(!e)return;if(HW&&HW.tv){var c=hClock();e.textContent=c[0]+" · "+c[1]}else e.textContent=""}
function hwFetch(){if(!document.hidden&&Date.now()-HWT>8000){HWT=Date.now();api("/api/home").then(function(r){return r.json()}).then(function(j){j.t0=Date.now();HW=j;hdTick();if(cur=="sum")homeDraw()}).catch(function(){})}}
function hClock(){var A=HW;if(!A||!A.tv)return["--:--:--",t("Ora non impostata")];var p=function(x){return(x<10?"0":"")+x},ms=(A.te+(Date.now()-A.t0)/1000)*1000,d=new Date(ms+A.to*1000),H=d.getUTCHours(),ap="";
 if(A.tf){ap=H>=12?" PM":" AM";H=H%12||12}var Y=d.getUTCFullYear(),M=p(d.getUTCMonth()+1),D=p(d.getUTCDate());
 return[p(H)+":"+p(d.getUTCMinutes())+":"+p(d.getUTCSeconds())+ap,A.df==1?Y+"-"+M+"-"+D:A.df==2?M+"/"+D+"/"+Y:D+"/"+M+"/"+Y]}
setInterval(hdTick,1000);
function wid(tt,v,sub,go,ic){return'<div class="wid" role="button" tabindex="0" onclick="show(\''+go+'\')" onkeydown="if(event.key==\'Enter\')show(\''+go+'\')"><div class="wt"><svg class="i" aria-hidden="true"><use href="#'+ic+'"/></svg>'+esc(tt)+'</div><div class="wv">'+v+'</div>'+(sub?'<div class="ws">'+sub+'</div>':"")+'</div>'}
function homeWid(s,n){hwFetch();if(!SV.httpPort)svcLoad();
 var W=HW||{},pill=function(on,a,b){return'<span class="pill'+(on?" a":"")+'">'+esc(on?a:b)+'</span>'},c=[],now=Math.floor(Date.now()/1000);
 if(SV.httpOn)c.push(wid("HTTP",esc(tf("Porta: {0}",SV.httpPort)),"","http","i-globe"));
 if(SV.httpsOn)c.push(wid("HTTPS",esc(tf("Porta: {0}",SV.httpsPort)),"","https","i-lock"));
 if(s.mqtt&&canSee("mqtt"))c.push(wid("MQTT",pill(s.mqtt==2,t("collegato"),t("non collegato")),"","mqtt","i-mqtt"));
 if(W.mesh&&canSee("mesh"))c.push(wid("ESP-NOW",esc(t(MROLE[W.role]||"Nodo")),esc(t("Schede vicine"))+": "+W.nodes,"mesh","i-mesh"));
 if(n.mdnsRun)c.push(wid("mDNS",esc(n.fqdn),"","mdns","i-tag"));
 if(W.ble)c.push(W.bleLim?wid("Bluetooth",esc(durText(W.bleLeft)),bar(Math.min(100,Math.round(100*W.bleLeft/(W.bleTot||600)))),"ble","i-link"):wid("Bluetooth",esc(t("Acceso")),"","ble","i-link"));
 if(W.ntp)c.push(wid("NTP",W.last?esc(tf("{0} fa",durText(Math.max(1,now-W.last)))):esc(t("Nessuna sincronizzazione ancora")),"","ntp","i-clock"));
 if(n.dhcpRun)c.push(wid("DHCP",pill(1,t("Attivo"),""),esc(t("Dispositivi collegati"))+": "+(n.clients||0),"dhcp","i-net"));
 if(n.st==1)c.push(wid(t("Punto di accesso (AP)"),(n.clients||0)+"",esc(t("Dispositivi collegati")),"ap","i-ant"));
 if(W.pw)c.push(wid(t("Risparmio energia"),esc(W.pw==1?t("Risparmio Wi-Fi"):t("Sonno a cicli")),"","power","i-leaf"));
 if(W.stat)c.push(wid(t("Statistiche"),pill(1,t("Attivo"),""),"","stat","i-chart"));
 return c.length?'<div class="wg">'+c.join("")+'</div>':""}
function identify(){post("/api/identify",{}).then(function(r){return r.json()}).then(function(j){msg("idm",j.ok?t("Guarda il LED: lampeggia a colori per 10 secondi"):j.err,j.ok);setTimeout(function(){var e=$("idm");if(e)e.className="msg"},5000)}).catch(function(){})}
function fillSec(){$("pwho").textContent=tf("Utente: {0} ({1})",USER,t(RNAME[ROLE]));if(ROLE<2)return;api("/api/settings").then(function(r){return r.json()}).then(function(c){SER=c.serialAuth;serDraw()});srLoad();banLoad()}
function banLoad(){api("/api/ban").then(function(r){return r.json()}).then(function(j){$("b_f").value=j.fails;$("b_s").value=j.secs;
 var h='<thead><tr><th>'+esc(t("Indirizzo"))+'</th><th>'+esc(t("Stato"))+'</th><th class="fac"></th></tr></thead><tbody>';
 if(!j.list.length)h+='<tr><td colspan="3" style="color:var(--mut)">'+esc(t("Nessun indirizzo bloccato o sospetto"))+'</td></tr>';
 j.list.forEach(function(b){h+='<tr><td><code>'+esc(b.ip)+'</code></td><td>'+(b.wait?'<span class="pill k">'+esc(tf("bloccato ancora {0}",durText(b.wait)))+'</span>':esc(tf("{0} errori",b.fails)))+'</td><td class="fac"><button class="btn gray sm" onclick="banUn(\''+esc(b.ip)+'\')">'+esc(t("Sblocca"))+'</button></td></tr>'});
 $("bntab").innerHTML=h+'</tbody>'}).catch(function(){})}
function banUn(ip){post("/api/ban/unban",{ip:ip}).then(function(r){return r.json()}).then(function(j){msg("bnm",j.ok?t("Sbloccato"):j.err,j.ok);banLoad()})}
function banSave(){post("/api/ban/set",{fails:$("b_f").value,secs:$("b_s").value}).then(function(r){return r.json()}).then(function(j){msg("bnm",j.ok?t("Salvato"):j.err,j.ok)})}
var SRD={};
function srOpts(){if($("sr_baud").options.length)return;[9600,19200,38400,57600,115200,230400,460800,921600].forEach(function(b){var o=document.createElement("option");o.value=b;o.textContent=b;$("sr_baud").appendChild(o)});[["crlf","CR+LF"],["lf","LF"],["cr","CR"]].forEach(function(e){var o=document.createElement("option");o.value=e[0];o.textContent=e[1];$("sr_eol").appendChild(o)})}
function srLoad(){srOpts();api("/api/serial").then(function(r){return r.json()}).then(function(j){SRD=j;srDraw()}).catch(function(){})}
function srDraw(){$("sr_baud").value=SRD.baud;$("sr_eol").value=SRD.eol;$("sr_tx").value=SRD.tx;["echo","input","log","banner"].forEach(function(k){$("sw_sr_"+k).className="sw1"+(SRD[k]?" on":"")});
 $("sr_usb").textContent=SRD.usb?t("USB nativa: la velocita non conta."):"";$("sr_trial").className=SRD.trial?"msg ko":"msg ko hide";clearTimeout(srDraw._t);if(SRD.trial)srDraw._t=setTimeout(srLoad,21000)}
function srSet(k,v){post("/api/serial",{k:k,v:v}).then(function(r){return r.json()}).then(function(j){if(j.err){msg("srm",j.err,false);srLoad()}else{SRD=j;srDraw();msg("srm",t("Salvato"),true)}}).catch(function(){})}
function srTog(k){if(k==="input"&&SRD.input&&!confirm(t("La seriale non accettera piu comandi. Si riattiva da questa pagina o con il tasto BOOT. Continuare?")))return;srSet(k,SRD[k]?"off":"on")}
var SER=true;
function serDraw(){$("sw_ser").className="sw1"+(SER?" on":"");var w=$("serwarn");if(SER){w.className="msg";w.textContent=""}else{w.className="msg ko";w.textContent=t("Attenzione: la seriale e aperta, chi ha il cavo USB puo usare la shell senza password.")}}
function serToggle(){var nv=!SER;if(!nv&&!confirm(t("Spegnere la password sulla seriale? Chi collega il cavo USB potra usare la shell senza password.")))return;
 post("/api/serialauth",{on:nv?"1":"0"}).then(function(r){return r.json()}).then(function(j){if(j.ok){SER=nv;serDraw();msg("serm",t("Salvato"),true);setTimeout(function(){$("serm").className="msg"},3000)}else msg("serm",j.err,false)}).catch(function(){})}
function draw(id,a,mn,mx){var c=$(id),x=c.getContext("2d");x.clearRect(0,0,c.width,c.height);if(!a||a.length<2)return;
 if(mn===null){mn=Math.min.apply(null,a)-2;mx=Math.max.apply(null,a)+2}
 x.strokeStyle="#3fa7ff";x.lineWidth=2;x.beginPath();
 for(var i=0;i<a.length;i++){var X=i*c.width/59,Y=c.height-(a[i]-mn)/(mx-mn)*c.height;if(i)x.lineTo(X,Y);else x.moveTo(X,Y)}x.stroke();
 x.fillStyle="#8b98a8";x.font="12px sans-serif";x.fillText(mx.toFixed(0),4,12);x.fillText(mn.toFixed(0),4,c.height-4)}
function scan(){$("scanmsg").textContent=t("Ricerca in corso...");api("/api/wifi/scan",{method:"POST"}).then(function(){setTimeout(scanRes,3000)})}
function scanRes(){api("/api/wifi/scan").then(function(r){return r.json()}).then(function(j){
 if(j.running){setTimeout(scanRes,1500);return}$("scanmsg").textContent=tf("{0} reti trovate",j.list.length);
 var h="<tr><th>"+t("Rete")+"</th><th>"+t("Segnale")+"</th><th>"+t("Canale")+"</th><th></th></tr>";
 j.list.forEach(function(n,i){h+="<tr><td>"+esc(n.ssid)+(n.open?" ("+t("aperta")+")":"")+"</td><td>"+n.rssi+" dBm</td><td>"+n.ch+'</td><td><button class="btn gray" onclick="pick('+i+')">'+t("Scegli")+'</button></td></tr>'});
 $("scanlist").innerHTML=h;window._sc=j.list})}
function pick(i){$("w_ssid").value=window._sc[i].ssid;$("w_pass").focus()}

var fp="/";
var OTZ=[];
function oraTz(){Promise.all([commonLoad(),api("/api/region").then(function(r){return r.json()})]).then(function(a){var r=a[1],c=COMMON.by[r.country],L=c?c.tz.slice():[],h="",f=-1;if(!L.some(function(z){return z[0]=="UTC"}))L.push(["UTC","UTC0"]);
 L.forEach(function(z,i){if(z[1]==r.tz&&(f<0||z[0]==r.tzName))f=i});if(f<0){L.push([r.tzName,r.tz]);f=L.length-1}
 L.forEach(function(z,i){h+='<option value="'+i+'">'+esc(z[0])+'</option>'});OTZ=L;$("o_tz").innerHTML=h;$("o_tz").value=f}).catch(function(){})}
function oraTzSave(){var z=OTZ[+$("o_tz").value];if(!z)return;post("/api/region",{tz:z[1],tzname:z[0]}).then(function(r){return r.json()}).then(function(j){msg("om4",j.ok===false?j.err:t("Salvato"),j.ok!==false);loadTime();oraTz()}).catch(function(){})}
function loadTime(){if(!$("o_tz").options.length)oraTz();api("/api/time").then(function(r){return r.json()}).then(function(t){
 $("oranow").textContent=t.valid?t.now:window.t("Ora non impostata");svcSt("ntp",t.ntp);$("o_ev").value=t.every;var ld=new Date((t.last||0)*1000);$("olast").textContent=t.last?tf("Ultima sincronizzazione: {0}",ld.toLocaleString()):(t.ntp?window.t("Nessuna sincronizzazione ancora"):window.t("Sincronizzazione spenta: ora a mano"));if(t.valid&&!$("o_md").value){var e=new Date(t.epoch*1000);$("o_md").value=e.getFullYear()+"-"+("0"+(e.getMonth()+1)).slice(-2)+"-"+("0"+e.getDate()).slice(-2);$("o_mt").value=("0"+e.getHours()).slice(-2)+":"+("0"+e.getMinutes()).slice(-2)+":00"}$("o_srv").value=t.server;$("o_srv2").value=t.server2||"";$("ontpwho").textContent=tf("Ogni richiesta NTP mostra ai server l'indirizzo IP pubblico della tua rete. Server usati: {0}",t.server+(t.server2?", "+t.server2:""));if($("ontpst"))$("ontpst").textContent=t.ntp?(t.last?tf("Ultima sincronizzazione: {0}",ld.toLocaleString()):window.t("Nessuna sincronizzazione ancora")):window.t("Sincronizzazione spenta: ora a mano");$("o_serve").value=t.serve?"1":"0";
 })}
function setManual(){var d=$("o_md").value,h=$("o_mt").value;if(!d||!h){msg("om3",t("Scegli data e ora"),false);return}if(h.length==5)h+=":00";
 post("/api/time/set",{local:d+" "+h}).then(function(r){return r.json()}).then(function(j){msg("om3",j.ok?t("Ora impostata"):j.err,j.ok);if(j.ok)loadTime()})}
function saveTime(on){if(on===undefined)on=!!(SVS.ntp&&SVS.ntp.on);return post("/api/time",{ntp:on?"1":"0",every:$("o_ev").value,server:$("o_srv").value,server2:$("o_srv2").value,serve:$("o_serve").value})
 .then(function(r){return r.json()}).then(function(j){msg("om",j.ok?t("Salvato"):j.err,j.ok);if(j.ok)setTimeout(loadTime,1500)})}
function syncNow(){post("/api/time/sync",{}).then(function(r){return r.json()}).then(function(j){msg("om2",j.ok?t("Sincronizzazione richiesta: controllo l'ora tra pochi secondi"):j.err,j.ok);if(j.ok){setTimeout(loadTime,3000);setTimeout(loadTime,8000)}})}
function setFromBrowser(){post("/api/time/set",{epoch:Math.floor(Date.now()/1000)}).then(function(r){return r.json()}).then(function(j){msg("om2",j.ok?t("Ora impostata"):j.err,j.ok);if(j.ok)loadTime()})}
function setCpu(){post("/api/cpu",{mode:$("cpumode").value}).then(function(r){return r.json()}).then(function(j){msg("cpum",j.ok?t("Salvato"):j.err,j.ok)})}
function enc(s){return encodeURIComponent(s)}
function join(a,b){return a=="/"?"/"+b:a+"/"+b}
