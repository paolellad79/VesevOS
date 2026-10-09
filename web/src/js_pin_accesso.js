/* funzioni scritte sulla foto della scheda: l'ESP32-S3 puo quasi tutto su ogni pin, quindi sono indicative */
var PFN={1:"UART PWM I2S ADC I2C SPI",2:"UART PWM I2S ADC I2C SPI",3:"UART PWM I2S ADC I2C SPI",4:"UART PWM I2S ADC I2C SPI",5:"UART PWM I2S ADC I2C SPI",6:"UART PWM I2S ADC I2C SPI",7:"UART PWM I2S ADC I2C SPI",8:"SPI I2C ADC I2S PWM UART",9:"SPI I2C ADC I2S PWM UART",10:"SPI I2C ADC I2S PWM UART",11:"SPI I2C ADC I2S PWM UART",12:"SPI I2C ADC I2S PWM UART",13:"SPI I2C ADC I2S PWM UART",14:"UART PWM I2S ADC I2C SPI",15:"UART PWM I2S ADC I2C SPI",16:"UART PWM I2S ADC I2C SPI",17:"GPIO",18:"GPIO",21:"GPIO",33:"UART PWM I2S I2C SPI",34:"UART PWM I2S I2C SPI",35:"SPI I2C I2S PWM UART",36:"SPI I2C I2S PWM UART",37:"UART PWM I2S I2C SPI",38:"UART PWM I2S I2C SPI",39:"SPI I2C I2S PWM UART",40:"SPI I2C I2S PWM UART",41:"SPI I2C I2S PWM UART",42:"SPI I2C I2S PWM UART",43:"UART0 TX",44:"UART0 RX",45:"SPI I2C I2S PWM UART",46:"SPI I2C I2S PWM UART",47:"SPI I2C I2S PWM UART",48:"SPI I2C I2S PWM UART",0:"BOOT"};
function pinWarnTxt(g){var w=[];if(g==3||g==45||g==46||g==0)w.push(t("Pin di avvio (strapping): meglio non usarlo"));if(g>=11&&g<=16)w.push(t("ADC2: con il Wi-Fi acceso la lettura analogica non e affidabile"));if(g>=39&&g<=42)w.push(t("Usato dal debug JTAG se lo si usa"));if(g==19||g==20)w.push(t("USB: non usabile"));if(g==43||g==44)w.push(t("Console seriale: evitare"));if(g>=33&&g<=37)w.push(t("Libero con la PSRAM da 2 MB di questa scheda; occupato sulle schede con PSRAM octal"));return w.join(". ")}
function pinPos(g){return PFL.indexOf(String(g))>=0||PFR.indexOf(String(g))>=0?t("bordo"):t("retro (piazzola)")}
function loadPins(){api("/api/pins/info").then(function(r){return r.json()}).then(function(b){PBI=b;$("pinchip").textContent=tf("Rilevato: {0} rev {1}, {2} core, {3} GPIO nel chip ({4} usabili come uscita). Scheda: {5}.",b.chip,b.rev,b.cores,b.gpioCount,b.gpioOut,b.board);drawPins()}).catch(function(){});
 api("/api/pins").then(function(r){return r.json()}).then(function(p){PUSE=p;
 var h="<tr><th>GPIO</th><th>"+t("Usato da")+"</th><th>"+t("Descrizione")+"</th></tr>";
 p.forEach(function(x){h+="<tr><td>"+x.gpio+"</td><td>"+esc(x.owner)+"</td><td>"+esc(x.note)+"</td></tr>"});
 $("pintab").innerHTML=h;if(PM.length)drawPins()});
 api("/api/pinmap").then(function(r){return r.json()}).then(function(m){PM=m;drawPins();pinPanel();pinInv()});
 api("/api/pinnotes").then(function(r){return r.json()}).then(function(n){PNOTE=n;drawPins();pinInv()});
 pinViewBtns();
 clearInterval(PTM);PTM=setInterval(pinPoll,1000);pinPoll()}
function pinViewBtns(){$("pinview").innerHTML='<button class="'+(PV=="f"?"on":"")+'" onclick="pinV(\'f\')">'+esc(t("Fronte"))+'</button><button class="'+(PV=="b"?"on":"")+'" onclick="pinV(\'b\')">'+esc(t("Retro"))+'</button>'}
function pinV(v){PV=v;pinViewBtns();drawPins()}
function pinInfo(g){for(var i=0;i<PM.length;i++)if(PM[i].g==g)return PM[i];return null}
var PUSE=[];
function pinOwner(g){for(var i=0;i<PUSE.length;i++)if(PUSE[i].gpio==g)return PUSE[i];return null}
function drawPins(){var W=400,bx=140,bw=120,top=56,st=30,bh=10*st+50,s="",ink="#8b98a8";
 var fr=PV=="f",L=fr?PFL:PFR,R=fr?PFR:PFL;
 s+='<rect x="'+bx+'" y="'+top+'" width="'+bw+'" height="'+bh+'" rx="12" fill="'+(fr?"#1f2a37":"#243246")+'" stroke="#3a4a5e"/>';
 s+='<rect x="'+(bx+bw/2-22)+'" y="'+(top-14)+'" width="44" height="22" rx="6" fill="#9aa5b1" stroke="#5c6670"/><text x="'+(bx+bw/2)+'" y="'+(top+2)+'" font-size="8" text-anchor="middle" fill="#1f2a37">USB-C</text>';
 s+='<text x="'+(bx+bw/2)+'" y="'+(top-22)+'" font-size="10" font-weight="700" text-anchor="middle" fill="'+ink+'">'+esc(fr?t("FRONTE"):t("RETRO (vista da sotto)"))+'</text>';
 if(fr){
  s+='<rect x="'+(bx+16)+'" y="'+(top+14)+'" width="30" height="14" rx="3" fill="#46566a"/><text x="'+(bx+31)+'" y="'+(top+24)+'" font-size="7" text-anchor="middle" fill="#fff">BOOT</text>';
  s+='<rect x="'+(bx+bw-46)+'" y="'+(top+14)+'" width="30" height="14" rx="3" fill="#46566a"/><text x="'+(bx+bw-31)+'" y="'+(top+24)+'" font-size="7" text-anchor="middle" fill="#fff">RESET</text>';
  var i48=pinInfo(48),c48=48==PT.gpio?"#e0a020":(i48&&i48.owner)?"#3fa7ff":"#3a4352";
  s+='<g style="cursor:pointer" onclick="pinSel(48)" role="button" aria-label="GPIO 48 LED RGB"><circle cx="'+(bx+bw/2)+'" cy="'+(top+44)+'" r="6" fill="'+c48+'" stroke="'+(PSEL==48?"#fff":"#0b0f14")+'"/><title>GPIO48 - LED RGB</title></g><text x="'+(bx+bw/2)+'" y="'+(top+62)+'" font-size="7" text-anchor="middle" fill="'+ink+'">LED RGB 48</text>';
  s+='<circle cx="'+(bx+bw/2+28)+'" cy="'+(top+44)+'" r="3" fill="#c0392b"/><text x="'+(bx+bw/2+28)+'" y="'+(top+62)+'" font-size="6" text-anchor="middle" fill="'+ink+'">'+esc(t("LED batteria"))+'</text>';
  s+='<rect x="'+(bx+30)+'" y="'+(top+110)+'" width="60" height="60" rx="4" fill="#2b3644" stroke="#46566a"/><text x="'+(bx+60)+'" y="'+(top+143)+'" font-size="9" text-anchor="middle" fill="'+ink+'">'+esc((PBI&&PBI.chip)||"ESP32-S3")+'</text>';
  s+='<rect x="'+(bx+14)+'" y="'+(top+bh-34)+'" width="92" height="24" rx="5" fill="#e58fb3"/><text x="'+(bx+60)+'" y="'+(top+bh-18)+'" font-size="8" text-anchor="middle" fill="#5a1b35">'+esc(t("ANTENNA"))+'</text>'}
 else{
  ["B+","B-","BOOST"].forEach(function(l,k){var x=bx+22+k*34;s+='<rect x="'+x+'" y="'+(top+bh-30)+'" width="28" height="16" rx="3" fill="'+(l=="B+"?"#d64545":l=="B-"?"#222":"#7a6a2a")+'" stroke="#555"/><text x="'+(x+14)+'" y="'+(top+bh-19)+'" font-size="7" text-anchor="middle" fill="#fff">'+l+'</text>'});
  s+='<text x="'+(bx+bw/2)+'" y="'+(top+bh-36)+'" font-size="6.5" text-anchor="middle" fill="'+ink+'">'+esc(t("BOOST: solo con batteria oltre 500 mAh"))+'</text>'}
 function col(g,x2,u){var nt=PNOTE[g];return g==PT.gpio?"#e0a020":x2.ok?(nt?"#9b6bd6":"#2e9e5b"):(x2.owner||u)?"#3fa7ff":nt?"#9b6bd6":"#3a4352"}
 function tip(g,x2,u){return"GPIO"+g+" - "+(PFN[g]||"")+(x2.why?" - "+x2.why:"")+(u?" - "+u.owner:"")+(PNOTE[g]?" - "+PNOTE[g]:"")+(pinWarnTxt(g)?" - "+pinWarnTxt(g):"")}
 function one(lab,x,y,side){var g=+lab,pw=PWR[lab];if(pw){s+='<circle cx="'+x+'" cy="'+y+'" r="11" fill="'+pw+'" stroke="#555"/><text x="'+x+'" y="'+(y+3)+'" font-size="7.5" text-anchor="middle" fill="#fff">'+lab+'</text>';return}
  var x2=pinInfo(g)||{g:g,ok:false},u=pinOwner(g),c=col(g,x2,u),nm=g==43?"TX":g==44?"RX":"";
  s+='<g style="cursor:pointer" onclick="pinSel('+g+')" role="button" aria-label="GPIO '+g+'"><circle cx="'+x+'" cy="'+y+'" r="11" fill="'+c+'" stroke="'+(g==PSEL?"#fff":"#0b0f14")+'" stroke-width="'+(g==PSEL?2.5:1)+'"/><text x="'+x+'" y="'+(y+3)+'" font-size="8.5" font-weight="700" text-anchor="middle" fill="#fff">'+g+'</text><title>'+esc(tip(g,x2,u))+'</title></g>';
  if(nm)s+='<text x="'+(x+(side<0?16:-16))+'" y="'+(y+3)+'" font-size="7" text-anchor="'+(side<0?"start":"end")+'" fill="'+ink+'">'+nm+'</text>';
  var lab2=PNOTE[g]||(u?u.owner+(u.note?": "+u.note:""):(g==PT.gpio?t("in prova"):""));
  if(lab2){s+='<path d="M'+(x+side*12)+' '+y+'H'+(x+side*24)+'" stroke="'+c+'" stroke-width="2"/><text x="'+(x+side*28)+'" y="'+(y+3)+'" font-size="8.5" text-anchor="'+(side<0?"end":"start")+'" fill="'+c+'">'+esc(lab2.length>18?lab2.slice(0,17)+"…":lab2)+'</text>'}}
 L.forEach(function(l,i){one(l,bx,top+40+i*st,-1)});R.forEach(function(l,i){one(l,bx+bw,top+40+i*st,1)});
 if(!fr){[[PIL,bx+30],[PIR,bx+bw-30]].forEach(function(cl){cl[0].forEach(function(g,i){var y=top+36+i*st*0.9,x2=pinInfo(g)||{g:g,ok:false},u=pinOwner(g),c=col(g,x2,u);
  s+='<g style="cursor:pointer" onclick="pinSel('+g+')" role="button" aria-label="GPIO '+g+'"><rect x="'+(cl[1]-12)+'" y="'+(y-9)+'" width="24" height="18" rx="4" fill="'+c+'" stroke="'+(g==PSEL?"#fff":"#0b0f14")+'" stroke-width="'+(g==PSEL?2.5:1)+'"/><text x="'+cl[1]+'" y="'+(y+3)+'" font-size="8" font-weight="700" text-anchor="middle" fill="#fff">'+g+'</text><title>'+esc(tip(g,x2,u))+'</title></g>'})})}
 var ly=top+bh+26;[["#2e9e5b",t("provabile")],["#3fa7ff",t("in uso")],["#9b6bd6",t("collegato (tuo nome)")],["#3a4352",t("vietato")],["#e0a020",t("in prova")]].forEach(function(l,i){var lx=14+i*78;s+='<circle cx="'+lx+'" cy="'+ly+'" r="5" fill="'+l[0]+'"/><text x="'+(lx+9)+'" y="'+(ly+3)+'" font-size="8" fill="'+ink+'">'+esc(l[1])+'</text>'});
 if(fr)s+='<text x="'+(W/2)+'" y="'+(ly+20)+'" font-size="8" text-anchor="middle" fill="'+ink+'">'+esc(t("Gli altri pin (piazzole) sono sul retro: premi Retro."))+'</text>';
 var e=$("pinsvg");e.setAttribute("viewBox","0 0 "+W+" "+(ly+30));e.innerHTML=s}
function pinInv(){var h="<tr><th>GPIO</th><th>"+esc(t("Posizione"))+"</th><th>"+esc(t("Funzioni"))+"</th><th>"+esc(t("Stato"))+"</th><th>"+esc(t("Collegato a"))+"</th></tr>",gs=PM.map(function(x){return x.g}).sort(function(a,b){return a-b});
 gs.forEach(function(g){var x=pinInfo(g),u=pinOwner(g),st=u?u.owner:(x.ok?t("libero"):(x.why||t("vietato"))),w=pinWarnTxt(g);
  h+="<tr><td><b>"+g+"</b></td><td>"+esc(pinPos(g))+"</td><td>"+esc(PFN[g]||"")+(w?'<div class="fhint">'+esc(w)+'</div>':"")+"</td><td>"+esc(st)+"</td><td>"+(ROLE>=2?'<input maxlength="24" value="'+esc(PNOTE[g]||"")+'" aria-label="GPIO '+g+'" onchange="pinNote('+g+',this)" style="min-width:120px">':esc(PNOTE[g]||""))+"</td></tr>"});
 $("pininv").innerHTML=h}
function pinNote(g,e){post("/api/pinnotes",{g:g,name:e.value}).then(function(r){return r.json()}).then(function(j){if(j.ok===false){msg("pinvm",j.err,false);return}PNOTE[g]=e.value.trim();if(!PNOTE[g])delete PNOTE[g];msg("pinvm",t("Salvato"),true);drawPins()}).catch(function(){})}
function pinSvgDl(){var x='<?xml version="1.0" encoding="UTF-8"?>\n<svg xmlns="http://www.w3.org/2000/svg" viewBox="'+$("pinsvg").getAttribute("viewBox")+'" font-family="Arial,sans-serif"><rect width="100%" height="100%" fill="#10151c"/>'+$("pinsvg").innerHTML+'</svg>';
 var a=document.createElement("a");a.href=URL.createObjectURL(new Blob([x],{type:"image/svg+xml"}));a.download="vesevos-pin.svg";document.body.appendChild(a);a.click();setTimeout(function(){URL.revokeObjectURL(a.href);a.remove()},500)}
function pinSel(g){PSEL=g;drawPins();pinPanel()}
function pinPanel(){var e=$("pinpanel"),h="";
 h+='<label for="pn_g">'+esc(t("Altro GPIO"))+'</label><input type="number" id="pn_g" min="0" max="48" value="'+(PSEL<0?"":PSEL)+'" style="width:90px" onchange="pinSel(parseInt(this.value))"> ';
 var x=pinInfo(PSEL);
 if(PSEL<0){e.innerHTML=h;return}
 h+='<h3 style="margin-top:10px">GPIO'+PSEL+'</h3>';if(PFN[PSEL])h+='<div class="fhint">'+esc(PFN[PSEL])+(pinWarnTxt(PSEL)?" - "+esc(pinWarnTxt(PSEL)):"")+'</div>';
 if(PNOTE[PSEL])h+='<div class="law">'+esc(tf("Segnato come collegato a: {0}. Una prova puo disturbare il circuito.",PNOTE[PSEL]))+'</div>';
 if(!x){h+='<div class="msg ko">'+esc(t("Pin inesistente su questa scheda"))+'</div>'}
 else if(!x.ok&&PSEL!=PT.gpio){h+='<div class="msg ko">'+esc(t("Non provabile:"))+" "+esc(x.why)+'</div>'}
 else{
  h+='<div id="pnlv" style="margin:6px 0"></div>';
  h+='<button class="btn" onclick="pinGo(\'high\')">'+esc(t("Alto (3,3 V)"))+'</button><button class="btn gray" onclick="pinGo(\'low\')">'+esc(t("Basso (0 V)"))+'</button><button class="btn" onclick="pinGo(\'blink\')">'+esc(t("Lampeggia"))+'</button>';
  h+='<div style="margin-top:8px"><button class="btn" onclick="pinGo(\'read\')">'+esc(t("Leggi"))+'</button> <select id="pn_pull"><option value="none">'+esc(t("senza resistenza"))+'</option><option value="up">'+esc(t("resistenza verso 3,3 V"))+'</option><option value="down">'+esc(t("resistenza verso massa"))+'</option></select></div>';
  h+='<div style="margin-top:8px"><button class="btn red" onclick="pinGo(\'off\')">'+esc(t("Rilascia"))+'</button></div>';
  h+='<div id="pnm" class="msg"></div>'}
 e.innerHTML=h;pinLevel()}
function pinWarn(){if(PWARN)return true;
 var w=[t("ATTENZIONE: una prova sbagliata puo danneggiare la scheda."),"",t("- Usa solo 3,3 V. Mai 5 V o tensioni piu alte su un pin."),t("- Ogni pin regge pochi mA: un LED vuole una resistenza da 220-470 ohm."),t("- Motori, relè e carichi grandi vanno pilotati con un transistor."),t("- Non collegare due uscite insieme e non toccare i pin di alimentazione."),"",t("Vuoi continuare?")].join("\n");
 if(!confirm(w))return false;PWARN=true;return true}
function pinGo(a){if(PSEL<0)return;if(a!="off"&&!pinWarn())return;
 var d={gpio:PSEL,action:a};if(a=="read")d.pull=$("pn_pull").value;
 post("/api/pintest",d).then(function(r){return r.json()}).then(function(j){
  if(!j.ok){msg("pnm",j.err,false);return}setTimeout(pinPoll,300)})}
function pinPoll(){if(cur!="pin"){clearInterval(PTM);return}
 api("/api/pintest").then(function(r){return r.json()}).then(function(j){var ch=(j.gpio!=PT.gpio);PT=j;
  if(ch){api("/api/pinmap").then(function(r){return r.json()}).then(function(m){PM=m;drawPins();pinPanel()})}else pinLevel()}).catch(function(){})}
function pinLevel(){var e=$("pnlv");if(!e)return;
 if(PT.gpio!=PSEL){e.textContent=t("Nessuna prova in corso su questo pin");return}
 var an={high:t("Alto"),low:t("Basso"),blink:t("Lampeggia"),read:t("Lettura")}[PT.action];
 e.innerHTML=esc(an)+" - "+esc(t("livello letto"))+": <b>"+(PT.level?t("ALTO"):t("BASSO"))+"</b> - "+esc(tf("si spegne tra {0} s",PT.left))}
function fillSys(){api("/api/settings").then(function(r){return r.json()}).then(function(c){$("s_host").value=c.hostname;$("s_dom").value=c.domain;$("s_dn").textContent=c.staDhcp?t("In DHCP il router non comunica il dominio alla scheda: scrivilo tu se serve."):"";$("s_fq").textContent=t("Nome completo:")+" "+c.hostname+(c.domain?"."+c.domain:"")})}
function saveSys(){post("/api/system",{hostname:$("s_host").value,domain:$("s_dom").value}).then(function(r){return r.json()}).then(function(j){msg("sm",j.ok?t("Salvato"):j.err,j.ok);if(j.ok)fillSys()})}
function loadLangs(){
 api("/api/langs").then(function(r){return r.json()}).then(function(l){LANGS=l;fillLangSel();
  var h="<table><tr><th>"+t("Codice")+"</th><th>"+t("Lingua")+"</th><th></th></tr>";
  l.forEach(function(x){h+="<tr><td>"+esc(x.code)+"</td><td>"+flagImg(x)+" "+esc(x.name)+"</td><td>"+(x.code=="it"||x.code=="en"?esc(t("Predefinita (nel firmware)")):'<button class="btn red" onclick="delLang(\''+esc(x.code)+'\')">'+esc(t("Elimina"))+'</button>')+"</td></tr>"});
  $("lglist").innerHTML=h+"</table>"});
 api("/api/fs/list?path=/").then(function(r){return r.json()}).then(function(j){if(j.ok)$("lgsp").textContent=tf("Memoria interna: usati {0} su {1}",kb(j.used),kb(j.total))})}
function upLang(){var f=$("lgf").files[0];if(!f){msg("lgm",t("Scegli un file"),false);return}
 var m=f.name.match(/^([a-z0-9]{2,8})\.json$/);if(!m||m[1]=="it"||m[1]=="en"){msg("lgm",t("Il nome deve essere come en.json (codice di 2-8 lettere minuscole)"),false);return}
 var code=m[1],fd=new FormData();fd.append("file",f,f.name);
 api("/api/fs/up?dir=/lang",{method:"POST",body:fd}).then(function(r){return r.json()}).then(function(j){
  if(!j.ok){msg("lgm",j.err,false);return}
  msg("lgm",t("Lingua caricata"),true);$("lgf").value="";
  if(code==LANG)setLang(code);else loadLangs()})}
function delLang(c){if(!confirm(tf("Eliminare la lingua {0}?",c)))return;
 var go=function(){post("/api/fs/del",{path:"/lang/"+c+".json"}).then(function(r){return r.json()}).then(function(j){msg("lgm",j.ok?t("Fatto"):j.err,j.ok);loadLangs()})};
 if(c==LANG){post("/api/lang",{code:"it"}).then(function(){LANG="it";DICT={};try{localStorage.setItem("vl","it")}catch(e){}refreshView();go()})}else go()}
function loadLic(){fetch("/api/license").then(function(r){return r.json()}).then(function(l){
  var h="";l.forEach(function(x){h+='<button class="btn gray" onclick="showLic(\''+esc(x.id)+'\')">'+esc(x.title)+'</button>'});$("licbtn").innerHTML=h}).catch(function(){})}
function showLic(id){fetch("/api/license?id="+encodeURIComponent(id)).then(function(r){return r.text()}).then(function(x){var e=$("licx");e.textContent=x;e.className="";e.scrollTop=0})}
var LG={txt:"",tm:null};
function loadLog(){api("/api/log").then(function(r){return r.text()}).then(function(x){LG.txt=x;lgDraw()}).catch(function(){})}
var LGL={E:0,W:1,I:2,D:3},LGC=["lk","lw","",""];
function lgDraw(){var e=$("lg");if(!e)return;var q=($("lgq").value||"").toLowerCase(),mx=+$("lgsh").value,all=LG.txt.split("\n").filter(Boolean),L=all.filter(function(l){var m=l.match(/\[([EWID])\] /),lv=m?LGL[m[1]]:2;return lv<=mx&&(!q||l.toLowerCase().indexOf(q)>=0)});
 var stick=e.scrollTop+e.clientHeight>=e.scrollHeight-20;e.innerHTML=L.map(function(l){var m=l.match(/\[([EWID])\] /),c=m?LGC[LGL[m[1]]]:"";return c?'<span class="'+c+'">'+esc(l)+'</span>':esc(l)}).join("\n");
 if(stick)e.scrollTop=e.scrollHeight;$("lginfo").textContent=L.length!=all.length?tf("{0} righe su {1}",L.length,all.length):tf("{0} righe",all.length)}
function lgInit(){api("/api/log/level").then(function(r){return r.json()}).then(function(j){$("lglv").value=j.lv;$("lgdw").className=j.lv>=3?"fhint":"fhint hide"}).catch(function(){});$("lglv").disabled=ROLE<2;$("btclr").disabled=ROLE<2;loadBoots()}
function lgSetLv(){var v=+$("lglv").value;post("/api/log/level",{lv:v}).then(function(r){return r.json()}).then(function(j){if(j.ok){$("lgdw").className=v>=3?"fhint":"fhint hide";loadLog()}})}
function loadBoots(){api("/api/boots").then(function(r){return r.text()}).then(function(x){$("bt").textContent=x||t("Nessun riavvio registrato")}).catch(function(){})}
function btClear(){dlg({title:t("Azzerare il diario dei riavvii?"),body:"",ok:t("Azzera il diario"),danger:true,onOk:function(){return post("/api/boots/clear",{}).then(function(r){return r.json()}).then(function(j){if(j.ok)loadBoots();return j.ok?"":j.err})}})}
function lgAuto(){clearInterval(LG.tm);if($("lgauto").checked&&cur=="log")LG.tm=setInterval(loadLog,3000)}
function lgDl(){var a=document.createElement("a");a.href=URL.createObjectURL(new Blob([LG.txt],{type:"text/plain"}));a.download="vesevos-log.txt";document.body.appendChild(a);a.click();setTimeout(function(){URL.revokeObjectURL(a.href);a.remove()},500)}
function lgClear(){dlg({title:t("Svuotare il registro?"),body:'<p style="margin:0">'+esc(t("Le righe vengono cancellate dalla memoria della scheda. Se ti servono, prima premi Scarica."))+'</p>',ok:t("Svuota"),danger:true,onOk:function(){return post("/api/log/clear",{}).then(function(r){return r.json()}).then(function(j){if(j.ok)loadLog();return j.ok?"":j.err})}})}
function restoreCfg(){var f=$("cf").files[0];if(!f){msg("cm",t("Scegli un file"),false);return}
 f.text().then(function(x){return post("/api/config/restore",{t:x,pw:$("bk_rp").value})}).then(function(r){return r.json()}).then(function(j){msg("cm",j.ok?tf("Ripristinato: {0} righe cambiate",j.changed):j.err,j.ok);if(j.ok)$("bk_rp").value=""})}
function bkSec(){$("bk_box").className=$("bk_sec").checked?"":"hide"}
function bkExport(){var p=$("bk_pw").value;
 if(p.length<10){msg("cm",t("La frase deve avere almeno 10 caratteri"),false);return}
 if(p!==$("bk_pw2").value){msg("cm",t("Le due frasi non sono uguali"),false);return}
 if(!$("bk_acc").checked){msg("cm",t("Devi accettare l'avviso per esportare i segreti"),false);return}
 post("/api/config/export",{acc:"1",pw:p}).then(function(r){return r.text()}).then(function(x){
  if(x.charAt(0)=="{"){var j={};try{j=JSON.parse(x)}catch(e){}msg("cm",j.err||t("Errore"),false);return}
  var a=document.createElement("a");a.href=URL.createObjectURL(new Blob([x],{type:"text/plain"}));a.download="vesevos-segreti.conf";document.body.appendChild(a);a.click();setTimeout(function(){URL.revokeObjectURL(a.href);a.remove()},500);
  $("bk_pw").value="";$("bk_pw2").value="";$("bk_acc").checked=false;msg("cm",t("Backup cifrato scaricato. Custodisci il file e la frase."),true)}).catch(function(){})}
function factory(){dlg({title:t("Azzerare TUTTO (anche la password)?"),body:'<p style="margin:0 0 8px">'+esc(t("Tutte le impostazioni, gli utenti e le password vengono cancellati. Per confermare scrivi la parola AZZERA."))+'</p><input id="fz_t" autocomplete="off" aria-label="'+esc(t("Parola di conferma"))+'">',ok:t("Azzera tutto"),danger:true,
 onOk:function(){if(dlgV("fz_t").toUpperCase()!==t("AZZERA"))return t("La parola non coincide");return post("/api/factory",{}).then(function(){msg("fzm",t("Riavvio in corso..."),true);return ""})}})}
function chPass(){post("/api/passwd",{o:$("p_old").value,n:$("p_new").value}).then(function(r){return r.json()}).then(function(j){msg("pm",j.ok?t("Password cambiata"):j.err,j.ok)})}
/* ===== 1.7.1: impronta della password (SHA-256 / HMAC), uguale alla scheda ===== */
var SHK=[0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2];
function utf8(s){return new TextEncoder().encode(s)}
function sha256b(m){var l=m.length,n=((l+9+63)>>6)<<6,b=new Uint8Array(n),W=new Int32Array(64),H=[0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19],i,j;
 b.set(m);b[l]=0x80;var bl=l*8;b[n-4]=bl>>>24;b[n-3]=bl>>>16;b[n-2]=bl>>>8;b[n-1]=bl;b[n-5]=Math.floor(l/536870912);
 for(i=0;i<n;i+=64){for(j=0;j<16;j++)W[j]=b[i+4*j]<<24|b[i+4*j+1]<<16|b[i+4*j+2]<<8|b[i+4*j+3];
  for(j=16;j<64;j++){var x=W[j-15],y=W[j-2];W[j]=(((x>>>7|x<<25)^(x>>>18|x<<14)^(x>>>3))+W[j-7]+((y>>>17|y<<15)^(y>>>19|y<<13)^(y>>>10))+W[j-16])|0}
  var a=H[0],c=H[1],d=H[2],e=H[3],f=H[4],g=H[5],h=H[6],k=H[7];
  for(j=0;j<64;j++){var t1=(k+((f>>>6|f<<26)^(f>>>11|f<<21)^(f>>>25|f<<7))+((f&g)^(~f&h))+SHK[j]+W[j])|0,t2=(((a>>>2|a<<30)^(a>>>13|a<<19)^(a>>>22|a<<10))+((a&c)^(a&d)^(c&d)))|0;
   k=h;h=g;g=f;f=(e+t1)|0;e=d;d=c;c=a;a=(t1+t2)|0}
  H[0]=H[0]+a|0;H[1]=H[1]+c|0;H[2]=H[2]+d|0;H[3]=H[3]+e|0;H[4]=H[4]+f|0;H[5]=H[5]+g|0;H[6]=H[6]+h|0;H[7]=H[7]+k|0}
 var o=new Uint8Array(32);for(i=0;i<8;i++){o[4*i]=H[i]>>>24;o[4*i+1]=H[i]>>>16;o[4*i+2]=H[i]>>>8;o[4*i+3]=H[i]}return o}
function hexb(b){var s="";for(var i=0;i<b.length;i++)s+=(b[i]<16?"0":"")+b[i].toString(16);return s}
function sha256Hex(s){return hexb(sha256b(utf8(s)))}
function hmacHex(key,msg){var k=utf8(key);if(k.length>64)k=sha256b(k);var ip=new Uint8Array(64),op=new Uint8Array(64),m=utf8(msg),i;
 for(i=0;i<64;i++){ip[i]=(k[i]||0)^0x36;op[i]=(k[i]||0)^0x5c}
 var a=new Uint8Array(64+m.length);a.set(ip);a.set(m,64);var ih=sha256b(a),z=new Uint8Array(96);z.set(op);z.set(ih,64);return hexb(sha256b(z))}
function hashPass(salt,p,it){var h=sha256Hex(salt+p);for(var i=0;i<it;i++)h=sha256Hex(h+salt+p);return h}

/* ===== utente, ruolo, accesso ===== */
var AUTH={set:true,setup:true},USER="",ROLE=2;
var RNAME=["Ospite","Operatore","Amministratore"];
function setRole(r){ROLE=+r;document.body.setAttribute("data-role",ROLE)}
function authLoad(){return fetch("/api/auth").then(function(r){return r.json()}).then(function(j){AUTH=j;AUTHSET=j.set;return j})}
var CLK={tm:0,t0:0};
function clkDraw(){var A=AUTH,e=$("lclk");if(!e)return;if(!A||A.te===undefined){e.textContent="";return}
 if(!A.tv){e.textContent=t("Ora della scheda non impostata. Si imposta dopo l'accesso (Sistema > Ora).");return}
 var ms=(A.te+(Date.now()-CLK.t0)/1000)*1000,d=new Date(ms+A.to*1000),p=function(n){return(n<10?"0":"")+n},
 Y=d.getUTCFullYear(),M=p(d.getUTCMonth()+1),D=p(d.getUTCDate()),H=d.getUTCHours(),ap="";
 if(A.tf){ap=H>=12?" PM":" AM";H=H%12||12}
 var ds=A.df==1?Y+"-"+M+"-"+D:A.df==2?M+"/"+D+"/"+Y:D+"/"+M+"/"+Y,
 of=A.to/60,sg=of<0?"-":"+",ao=Math.abs(of),tz="UTC"+sg+p(Math.floor(ao/60))+":"+p(ao%60);
 var s=tf("Ora della scheda: {0} {1} ({2})",ds,p(H)+":"+p(d.getUTCMinutes())+":"+p(d.getUTCSeconds())+ap,(A.tn?A.tn+", ":"")+tz);
 if(Math.abs(ms-Date.now())>30000)s+=" - "+t("Differisce dall'ora del tuo dispositivo: controlla data e fuso.");
 e.textContent=s}
function clkStart(){CLK.t0=Date.now();clearInterval(CLK.tm);clkDraw();CLK.tm=setInterval(clkDraw,1000)}
function showLoginText(){clkStart();$("lt").textContent=AUTH.set?t("Accesso"):t("Primo accesso");
 var n=!AUTH.set;$("lnew").className=n?"":"hide";$("lp2w").className=n?"":"hide";$("lbt").textContent=n?t("Salva ed entra"):t("Entra");
 $("lu").readOnly=n;if(n)$("lu").value=AUTH.first||"admin";$("lp").setAttribute("autocomplete",n?"new-password":"current-password");
 $("lhttps").textContent=AUTH.https&&!AUTH.secure?t("Collegamento non cifrato: usi l'hotspot della scheda, protetto dalla sua password Wi-Fi."):AUTH.secure?tf("Collegamento cifrato (HTTPS). Impronta del certificato: {0}",AUTH.fp):""}
function showLogin(){MFAT=null;if($("lmfa")){$("lmfa").className="hide";$("lbt").className="btn";$("lu").disabled=false;$("lp").disabled=false}clearInterval(timer);$("app").className="hide";$("login").className="card";document.body.classList.remove("cpo");setupClose();
 authLoad().then(function(){showLoginText();try{var u=localStorage.getItem("vu");if(u&&AUTH.set)$("lu").value=u}catch(e){}($("lu").value&&AUTH.set?$("lp"):$("lu")).focus()}).catch(function(){})}
function lKey(e){if(e.key=="Enter"){e.preventDefault();doLogin()}}
function loginOk(j){clearInterval(CLK.tm);if(j.clean)setTimeout(function(){dlg({title:t("Hotspot e HTTP spenti"),body:'<p style="margin:0">'+esc(t("Hai raggiunto la scheda dalla rete di casa: l'hotspot, il portale automatico e HTTP sono stati spenti. Per riaccenderli: Servizi. Se perdi la Wi-Fi: tasto BOOT 8 secondi."))+'</p>',buttons:[["ok",t("OK"),""]],onOk:function(){return""}})},800);$("lp").value="";$("lp2").value="";USER=j.user;setRole(j.role);try{localStorage.setItem("vu",USER)}catch(e){}
 authLoad().then(function(){start()}).catch(function(){start()})}
function doLogin(){var u=$("lu").value.trim(),p=$("lp").value;if(!p){msg("lm",t("Scrivi la password"),false);return}
 if(!AUTH.set){if(p.length<6){msg("lm",t("Password troppo corta (min 6)"),false);return}if(p!=$("lp2").value){msg("lm",t("Le due password non sono uguali"),false);return}
  post("/api/firstpass",{p:p}).then(function(r){return r.json()}).then(function(j){if(j.ok)loginOk(j);else msg("lm",j.err||t("Errore"),false)}).catch(function(){});return}
 if(!u){msg("lm",t("Scrivi il nome utente"),false);return}
 msg("lm",t("Controllo..."),true);
 post("/api/login/start",{u:u}).then(function(r){return r.json()}).then(function(s){if(s.ok===false){msg("lm",s.err,false);return}
  var mac=hmacHex(hashPass(s.salt,p,s.iter),s.nonce);
  msg("lm",s.pow?t("Verifico che sei una persona..."):t("Controllo..."),true);
  return powSolve(s.nonce,s.pow||0).then(function(pw){
  return post("/api/login",{u:u,nonce:s.nonce,mac:mac,pw:pw,hp:$("lhp").value}).then(function(r){return r.json()}).then(function(j){if(j.ok)loginOk(j);else if(j.mfa)mfaStep(j);else msg("lm",j.err||t("Errore"),false)})})}).catch(function(){})}
/* prova di lavoro anti-bot: sha256(nonce:n) con N zeri iniziali (a pezzi, la pagina non si blocca) */
function powSolve(nonce,bits){return new Promise(function(res){if(!bits){res("");return}var n=0,hx=Math.floor(bits/4),rest=bits%4;
 function ok(h){for(var i=0;i<hx;i++)if(h.charAt(i)!="0")return false;if(rest&&parseInt(h.charAt(hx),16)>=(16>>rest))return false;return true}
 (function step(){var e=n+1500;for(;n<e;n++){if(ok(sha256Hex(nonce+":"+n))){res(String(n));return}}setTimeout(step,0)})()})}
var MFAT=null;
function mfaStep(j){MFAT=j.tok;$("lmfa").className="";$("lbt").className="hide";$("lu").disabled=true;$("lp").disabled=true;$("lcode").value="";
 var n=$("lmfan");if(j.recOnly){n.className="law";n.textContent=t("L'ora della scheda non e valida: serve un codice di recupero.")}
 else if(j.notime){n.className="law";n.textContent=t("L'ora della scheda non e valida: con il codice giusto la scheda prende l'ora di questo dispositivo. Controlla che l'ora del dispositivo sia esatta.")}else n.className="hide";
 msg("lm","",true);setTimeout(function(){$("lcode").focus()},50)}
function doMfa(){var c=$("lcode").value.trim();if(!c){msg("lm",t("Scrivi il codice"),false);return}
 post("/api/login/mfa",{tok:MFAT,code:c,now:Math.floor(Date.now()/1000)}).then(function(r){return r.json()}).then(function(j){if(j.ok){$("lu").disabled=false;$("lp").disabled=false;$("lmfa").className="hide";$("lbt").className="btn";loginOk(j)}else{msg("lm",j.err||t("Errore"),false);if(/Tempo scaduto/.test(j.err||"")){setTimeout(showLogin,1500)}}}).catch(function(){})}
function logout(){post("/api/logout",{}).then(showLogin).catch(showLogin)}
function start(){$("login").className="hide";$("app").className="";setRole(AUTH.role===undefined?ROLE:AUTH.role);USER=AUTH.user||USER;
 $("who").textContent=USER?USER+" · "+t(RNAME[ROLE]):"";buildNav();cpApply();poll();rfApply();auPoll();
 if(!AUTH.setup&&ROLE==2)setupOpen();else if(location.hash=="#aiuto")show("help")}

/* ===== lingua: bandiere ===== */
function flagImg(l){return l&&l.flag?'<img class="flag" alt="" src="data:image/svg+xml;charset=utf-8,'+encodeURIComponent(l.flag)+'">':""}
function langOf(c){for(var i=0;i<LANGS.length;i++)if(LANGS[i].code==c)return LANGS[i];return{code:c,name:c}}
function fillLangSel(){var l=langOf(LANG),h=flagImg(l)+'<span>'+esc(LANG.toUpperCase())+'</span>';
 var bs=document.querySelectorAll(".lbtn");for(var i=0;i<bs.length;i++){bs[i].innerHTML=h;bs[i].title=t("Lingua")+": "+l.name;bs[i].setAttribute("aria-label",bs[i].title)}
 var s=$("lc_lang");if(s){var o="";LANGS.forEach(function(x){o+='<option value="'+esc(x.code)+'"'+(x.code==LANG?" selected":"")+'>'+esc(x.name)+'</option>'});s.innerHTML=o}}
function langPop(e){e.stopPropagation();var m=$("lpop");if(!m.classList.contains("hide")){m.className="pop hide";return}
 var h="";LANGS.forEach(function(x){h+='<button role="menuitem" class="'+(x.code==LANG?"on":"")+'" onclick="$(\'lpop\').className=\'pop hide\';setLang(\''+esc(x.code)+'\')">'+flagImg(x)+'<span><b>'+esc(x.name)+'</b><small>'+esc(x.loc||x.code)+'</small></span><i class="ck">&#10003;</i></button>'});
 h+='<div class="lloc" id="hlocn"></div>';m.innerHTML=h;homeLoc();var r=e.currentTarget.getBoundingClientRect();m.style.top=(r.bottom+6)+"px";m.style.right=Math.max(8,innerWidth-r.right)+"px";m.className="pop";$("thm").className="pop hide";$("um").className="pop hide"}
document.addEventListener("click",function(e){var m=$("lpop");if(m&&!m.classList.contains("hide")&&!e.target.closest("#lpop"))m.className="pop hide"});

/* ===== navigazione con ruoli ===== */
var TMIN={dmgr:1,btn:1,usb:1,temp:1,mem:1,cpu:1,rad:1,selftest:2,stat:1,power:1,cert:1,radio:1,ntp:1,dhcp:1,dns:1,mdns:1,ble:1,ap:1,http:1,wifi:1,mqtt:1,mesh:1,pin:1,led:1,task:1,file:1,ora:1,loc:1,auto:1,boot:1,wd:1,shell:1,audit:1,https:1,users:2,fw:2};
function canSee(id){if(AUTH&&AUTH.pk&&AUTH.pk[id]===0)return false;return ROLE>=(TMIN[id]||0)}
function gTabs(g){return GMAP[g].filter(canSee)}
function buildNav(){var h='<svg class="i slogo" style="stroke:url(#vg)"><use href="#i-chipn"/></svg>';
 GRP.forEach(function(g){if(gTabs(g[0]).length)h+='<button class="nb" id="n_'+g[0]+'" onclick="gGo(\''+g[0]+'\')"><svg class="i"><use href="#'+GICO[g[0]]+'"/></svg>'+esc(t(g[1]))+'</button>'});$("nav").innerHTML=h;
 $("bellb").title=t("Allarmi");$("outb").title=t("Logout, riavvia o sleep");$("tbt").classList.toggle("hide",ROLE<1);themeApply();cpTitles();show(canSee(cur)?cur:"sum")}
function gGo(g){var L=gTabs(g);show(LASTG[g]&&canSee(LASTG[g])?LASTG[g]:L[0])}
function show(id){if(!canSee(id))id="sum";cur=id;var g=gOf(id);LASTG[g]=id;
 TABS.forEach(function(x){var e=$("t_"+x[0]);if(e)e.className=x[0]==id?"":"hide"});
 GRP.forEach(function(x){var e=$("n_"+x[0]);if(e)e.className="nb"+(x[0]==g?" on":"")});
 var L=gTabs(g),h="";if(L.length>1){L.forEach(function(k,ix){var x=null;TABS.forEach(function(y){if(y[0]==k)x=y});if(!x)return;
  if(g=="sys"&&DIAG.indexOf(k)>=0&&(ix==0||DIAG.indexOf(L[ix-1])<0))h+='<span class="gsep">'+esc(t("Diagnostica"))+'</span>';
  if(g=="sys"&&DIAG.indexOf(k)<0&&ix>0&&DIAG.indexOf(L[ix-1])>=0)h+='<span class="gsep"></span>';
  h+='<button class="'+(k==id?"on":"")+'" onclick="show(\''+k+'\')">'+esc(t(x[1]))+'</button>'})}
 $("gsub").innerHTML=h;$("gsub").className="sub gsub"+(h?"":" hide");$("alerts").className="alerts hide";
 if(id=="dmgr")dmLoad();if(["btn","usb","temp","mem","cpu","rad"].indexOf(id)>=0)dvRows(id);if(id=="shell")cpToggle(true);if(id!="auto"){clearInterval(RU.tm)}if(id=="auto")rLoad();if(id=="boot")bLoad();if(id!="task")clearInterval(TK.tm);if(id=="task")loadTasks();if(id=="pin")loadPins();if(id=="loc")loadLangs();if(id=="log"){lgInit();loadLog();lgAuto()}else clearInterval(LG.tm);if(id=="wifi")fillNet();if(id=="led")fillLed();if(id=="ora")loadTime();if(id=="file")fList(fp);
 if(id=="sec")fillSec();if(id=="sum"){homeDraw()}if(id=="mqtt")mqLoad();if(id=="mesh")msLoad();if(id=="loc")lcLoad();if(id=="wd")wdLoad();if(id=="users")usLoad();if(id=="fw")fwLoad();if(id=="https"){tlsLoad();svcLoad()}if(id=="ap"){fillNet();svcLoad()}if(id=="http"||id=="dhcp"||id=="dns"||id=="mdns")svcLoad();if(id=="ntp")loadTime();if(id=="mfa")mfLoad();if(id!="stat")clearInterval(STT);if(id=="stat"){stLoad();clearInterval(STT);STT=setInterval(stLoad,5000)}if(id=="power")pwLoad();if(id=="selftest")stlPoll();if(id=="cert")tlsLoad();if(id=="ble"&&ROLE==2){bleLoad();clearInterval(BLT);BLT=setInterval(function(){if(cur!="ble")clearInterval(BLT);else bleLoad()},3000)}if(id=="radio")lcLoad();if(id=="audit")auLoad();if(id=="legal")lgLoad();
 if(id=="help"&&location.hash!="#aiuto")try{history.replaceState(null,"","#aiuto")}catch(e){}if(id!="help"&&location.hash=="#aiuto")try{history.replaceState(null,"",location.pathname)}catch(e){}}
window.addEventListener("hashchange",function(){if(location.hash=="#aiuto"&&loggedIn())show("help")});


