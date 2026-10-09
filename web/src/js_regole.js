/* ===== Automazioni ===== */
var RU={list:[],st:[],ed:null,edi:-1,tm:null,txt:""};
var DAYS=[["0","Lun"],["1","Mar"],["2","Mer"],["3","Gio"],["4","Ven"],["5","Sab"],["6","Dom"]];
var LEDM=[["state","stato del sistema"],["heartbeat","battito"],["fixed","colore fisso"],["off","spento"]];
var UNITS=[["1","secondi"],["60","minuti"],["3600","ore"]];
var ONOFF=[["1","acceso"],["0","spento"]];
function lk(a,v){for(var i=0;i<a.length;i++)if(String(a[i][0])==String(v))return t(a[i][1]);return v}
function rClean(x){return String(x).replace(/[|;\n\r]/g," ").replace(/\s+/g," ").trim()}
function aParse(a){var w=a.split(" "),k=w[0],v=w.slice(1).join(" ");
 if(k=="led-color")return{k:"color",v:v};if(k=="led")return{k:"ledmode",v:v};if(k=="led-bright")return{k:"bright",v:+v};if(k=="led2")return null;
 if(k=="gpio")return{k:"gpio",p:+w[1],v:+w[2]};if(k=="wait")return{k:"wait",v:+v};if(k=="note")return{k:"note",v:v};if(k=="reboot")return{k:"reboot"};if(k=="airplane")return{k:"air",v:v};if(k=="mqtt")return{k:"mq",p:w[1]||"",v:w.slice(2).join(" ")};if(k=="ntp")return{k:"ntp"};return null}
function aSer(a){switch(a.k){case"color":return"led-color "+String(a.v).replace("#","").toLowerCase();case"ledmode":return"led "+a.v;case"bright":return"led-bright "+a.v;
 case"gpio":return"gpio "+a.p+" "+a.v;case"wait":return"wait "+a.v;case"note":return"note "+rClean(a.v);case"reboot":return"reboot";case"air":return"airplane "+a.v;case"mq":return"mqtt "+(String(a.p).replace(/[\s|;]/g,"")||"evento")+" "+rClean(a.v);case"ntp":return"ntp sync"}return""}
function rParse(line){var f=line.split("|");if(f.length!=5)return null;
 var r={on:f[0].trim()=="1",name:f[1].trim(),tk:"time",h:"07:00",d:"1111111",n:60,tmp:70,ck:"-",c1:"22:00",c2:"06:00",cd:"1111111",acts:[]};
 var w=f[2].trim().split(/\s+/);
 if(w[0]=="time"){r.tk="time";r.h=w[1];r.d=w[2]}else if(w[0]=="every"||w[0]=="after"){r.tk=w[0];r.n=+w[1]}else if(w[0]=="boot")r.tk="boot";
 else if(w[0]=="wifi")r.tk=w[1]=="up"?"wifiup":"wifidown";else if(w[0]=="temp"){r.tk="temp";r.tmp=+w[1]}else return null;
 var c=f[3].trim().split(/\s+/);if(c[0]=="between"){r.ck="between";r.c1=c[1];r.c2=c[2]}else if(c[0]=="day"){r.ck="day";r.cd=c[1]}
 r.acts=f[4].split(";").map(function(a){return aParse(a.trim())}).filter(Boolean);return r}
function rSer(r){var tg=r.tk=="time"?"time "+r.h+" "+r.d:r.tk=="every"||r.tk=="after"?r.tk+" "+r.n:r.tk=="boot"?"boot":r.tk=="wifiup"?"wifi up":r.tk=="wifidown"?"wifi down":"temp "+r.tmp;
 var cd=r.ck=="between"?"between "+r.c1+" "+r.c2:r.ck=="day"?"day "+r.cd:"-";
 return(r.on?1:0)+"|"+rClean(r.name)+"|"+tg+"|"+cd+"|"+r.acts.map(aSer).join(";")}
function dayText(m){if(m=="1111111")return t("ogni giorno");if(m=="1111100")return t("dal lunedi al venerdi");if(m=="0000011")return t("sabato e domenica");
 var o=[];for(var i=0;i<7;i++)if(m[i]=="1")o.push(t(DAYS[i][1]));return o.join(", ")}
function durU(k,x){return k==0?tf("{0} g",x):k==1?tf("{0} h",x):k==2?tf("{0} min",x):tf("{0} s",x)}
function durText(n){n=Math.max(0,Math.round(n));var v=[Math.floor(n/86400),Math.floor(n/3600)%24,Math.floor(n/60)%60,n%60],i=0,r=[];while(i<3&&!v[i])i++;for(var k=i;k<Math.min(i+2,4);k++)if(v[k]||!r.length)r.push(durU(k,v[k]));return r.join(" ")}
function aText(a){switch(a.k){case"mq":return tf("MQTT: invia \"{1}\" su {0}",a.p,a.v);case"air":return tf("modo aereo: {0}",lk(AIRO,a.v));case"color":return tf("LED colore #{0}",String(a.v).replace("#",""));
 case"ledmode":return tf("LED: {0}",lk(LEDM,a.v));
 case"bright":return tf("luminosita LED {0}",a.v);
 case"gpio":return tf("pin {0} {1}",a.p,lk(ONOFF,a.v));case"wait":return tf("aspetta {0}",durText(a.v));
 case"note":return tf("scrivi \"{0}\" nel registro",a.v);case"reboot":return t("riavvia la scheda");case"ntp":return t("aggiorna l'ora")}return""}
function rSum(r){var q;
 if(r.tk=="time")q=tf("Alle {0}, {1}",r.h,dayText(r.d));else if(r.tk=="every")q=tf("Ogni {0}",durText(r.n));else if(r.tk=="after")q=tf("{0} dopo l'avvio",durText(r.n));
 else if(r.tk=="boot")q=t("All'avvio");else if(r.tk=="wifiup")q=t("Quando il Wi-Fi si collega");else if(r.tk=="wifidown")q=t("Quando il Wi-Fi si scollega");else q=tf("Quando la temperatura supera {0} C",r.tmp);
 if(r.ck=="between")q+=" - "+tf("solo dalle {0} alle {1}",r.c1,r.c2);else if(r.ck=="day")q+=" - "+tf("solo: {0}",dayText(r.cd));
 return q+": "+(r.acts.map(aText).join(", ")||"-")}
function rLoad(){clearInterval(RU.tm);api("/api/rules").then(function(r){return r.json()}).then(function(j){RU.txt=j.text;RU.st=j.st;
  RU.list=j.text.split("\n").map(function(l){return l.trim()}).filter(function(l){return l&&l[0]!="#"}).map(rParse).filter(Boolean);rDraw()}).catch(function(){});
 api("/api/time").then(function(r){return r.json()}).then(function(j){var e=$("atime");if(!j.valid){e.className="msg ko";e.textContent=t("L'ora non e impostata: le regole a orario aspettano. Imposta l'ora nella scheda Ora.")}else{e.className="msg";e.textContent=""}}).catch(function(){});
 RU.tm=setInterval(function(){if(RU.ed)return;api("/api/rules").then(function(r){return r.json()}).then(function(j){RU.st=j.st;if(!RU.ed)rDraw()}).catch(function(){})},3000)}
function rDraw(){var h="";if(!RU.list.length)h='<p style="color:var(--mut)">'+esc(t("Nessuna regola. Creane una o parti da un modello."))+'</p>';
 RU.list.forEach(function(r,i){var st=RU.st[i]||{},ex="";
  if(st.off)ex='<span class="pill" style="background:var(--kobg);color:var(--kot)">'+esc(t("Fermata (troppe esecuzioni)"))+'</span> ';else if(st.run)ex='<span class="pill v">'+esc(t("In corso"))+'</span> ';
  var last=st.last?tf("Ultima volta: {0} ({1} volte)",st.last,st.n):(st.n?tf("Eseguita {0} volte",st.n):t("Mai eseguita"));
  h+='<div class="ru"><button class="sw1'+(r.on?" on":"")+'" onclick="rToggle('+i+')" aria-label="'+esc(r.name)+'"></button><div class="rb"><b>'+esc(r.name)+'</b><div class="subt">'+esc(rSum(r))+'</div><div class="subt">'+ex+esc(last)+'</div></div>'+
  '<div class="rbt"><button class="btn gray" onclick="rRun('+i+')">'+esc(t("Prova ora"))+'</button><button class="btn gray" onclick="rEdit('+i+')">'+esc(t("Modifica"))+'</button><button class="btn gray" onclick="rDel('+i+')">'+esc(t("Elimina"))+'</button></div></div>'});
 $("rlist").innerHTML=h}
function rPush(okMsg){var txt=RU.list.map(rSer).join("\n");return post("/api/rules",{text:txt}).then(function(r){return r.json()}).then(function(j){
  if(j.ok){if(okMsg)msg("rm",okMsg,true);return rLoad(),true}msg("rm",j.err||t("Errore"),false);rLoad();return false})}
function rToggle(i){RU.list[i].on=!RU.list[i].on;rPush(t("Salvato"))}
function rDel(i){if(!confirm(tf("Eliminare la regola \"{0}\"?",RU.list[i].name)))return;RU.list.splice(i,1);rPush(t("Regola eliminata"))}
function rRun(i){post("/api/rules/run",{i:i}).then(function(r){return r.json()}).then(function(j){msg("rm",j.ok?t("Regola avviata"):j.err,j.ok);setTimeout(rLoad,1500)})}
function rBlank(){return{on:true,name:t("Nuova regola"),tk:"time",h:"07:00",d:"1111100",n:60,tmp:70,ck:"-",c1:"22:00",c2:"06:00",cd:"1111111",acts:[{k:"color",v:"ffcc00"}]}}
function rNew(){RU.edi=-1;RU.ed=rBlank();rEditDraw()}
function rEdit(i){RU.edi=i;RU.ed=JSON.parse(JSON.stringify(RU.list[i]));rEditDraw()}
function rTpl(k){var r=rBlank();
 if(k==0){r.name=t("Sveglia");r.tk="time";r.h="07:00";r.d="1111100";r.acts=[{k:"color",v:"ffcc00"},{k:"ledmode",v:"fixed"},{k:"note",v:t("Sveglia")}]}
 else if(k==1){r.name=t("Dopo l'avvio");r.tk="after";r.n=600;r.acts=[{k:"note",v:t("Sono passati 10 minuti dall'avvio")}]}
 else{r.name=t("Allarme temperatura");r.tk="temp";r.tmp=70;r.acts=[{k:"color",v:"ff0000"},{k:"ledmode",v:"heartbeat"},{k:"note",v:t("Temperatura alta")}]}
 RU.edi=-1;RU.ed=r;rEditDraw()}
function selOpts(list,cur){return list.map(function(o){return'<option value="'+o[0]+'"'+(o[0]==cur?" selected":"")+'>'+esc(t(o[1]))+'</option>'}).join("")}
function dayBtns(id,m){var h='<div class="dsel" id="'+id+'">';for(var i=0;i<7;i++)h+='<button type="button" class="'+(m[i]=="1"?"on":"")+'" onclick="this.classList.toggle(\'on\');rPrev()">'+esc(t(DAYS[i][1]))+'</button>';return h+'</div>'}
function dayMask(id){var o="";var b=$(id).children;for(var i=0;i<7;i++)o+=b[i].classList.contains("on")?"1":"0";return o}
var TKS=[["time","Orario (sveglia)"],["every","Ogni tanto (timer ripetuto)"],["after","Dopo l'avvio (una volta)"],["boot","All'avvio"],["wifiup","Quando il Wi-Fi si collega"],["wifidown","Quando il Wi-Fi si scollega"],["temp","Quando la temperatura supera"]];
var CKS=[["-","Sempre"],["between","Solo in una fascia oraria"],["day","Solo in certi giorni"]];
var AIRO=[["on boot","attivo fino al prossimo avvio"],["on 10m","attivo per 10 minuti"],["on 1h","attivo per 1 ora"],["on fisso","attivo, si spegne solo a mano"],["off","spento (rete accesa)"]];
var AKS=[["color","LED: colore"],["ledmode","LED: modo"],["bright","LED: luminosita"],["gpio","Pin: acceso o spento"],["wait","Aspetta"],["note","Scrivi nel registro"],["air","Modo aereo"],["mq","MQTT: invia un messaggio"],["ntp","Aggiorna l'ora"],["reboot","Riavvia la scheda"]];
function unitOf(n){return n%3600==0?3600:n%60==0?60:1}
function rEditDraw(){var r=RU.ed;
 $("redit").innerHTML='<h3>'+esc(RU.edi<0?t("Nuova regola"):t("Modifica regola"))+'</h3>'+
 '<label for="r_name">'+esc(t("Nome"))+'</label><input id="r_name" maxlength="24" value="'+esc(r.name)+'" oninput="rPrev()">'+
 '<h4>'+esc(t("Quando"))+'</h4><select id="r_tk" onchange="rTrigUi()">'+selOpts(TKS,r.tk)+'</select><div id="r_tf"></div>'+
 '<h4>'+esc(t("Se (facoltativo)"))+'</h4><select id="r_ck" onchange="rCondUi()">'+selOpts(CKS,r.ck)+'</select><div id="r_cf"></div>'+
 '<h4>'+esc(t("Allora"))+'</h4><div id="racts"></div><button class="btn gray" onclick="aAdd()">'+esc(t("Aggiungi azione"))+'</button>'+
 '<p id="rprev" class="subt" style="margin-top:12px"></p>'+
 '<button class="btn" onclick="rSave()">'+esc(t("Salva"))+'</button><button class="btn gray" onclick="rCancel()">'+esc(t("Annulla"))+'</button><div id="rem" class="msg"></div>';
 rTrigUi();rCondUi();aDraw();$("redit").className="card rcard";$("redit").scrollIntoView({behavior:"smooth",block:"start"})}
function rTrigUi(){var r=RU.ed,k=$("r_tk").value,h="";r.tk=k;
 if(k=="time")h='<label for="r_h">'+esc(t("Ora"))+'</label><input type="time" id="r_h" value="'+esc(r.h)+'" oninput="rPrev()"><label>'+esc(t("Giorni"))+'</label>'+dayBtns("r_d",r.d);
 else if(k=="every"||k=="after"){var u=unitOf(r.n);h='<label>'+esc(t("Tempo"))+'</label><div class="arow"><input type="number" id="r_n" min="1" value="'+(r.n/u)+'" oninput="rPrev()"><select id="r_u" onchange="rPrev()">'+selOpts(UNITS,u)+'</select></div>'}
 else if(k=="temp")h='<label for="r_tmp">'+esc(t("Temperatura (gradi C)"))+'</label><input type="number" id="r_tmp" min="30" max="110" value="'+r.tmp+'" oninput="rPrev()">';
 $("r_tf").innerHTML=h;rPrev()}
function rCondUi(){var r=RU.ed,k=$("r_ck").value,h="";r.ck=k;
 if(k=="between")h='<div class="arow"><span>'+esc(t("Dalle"))+'</span><input type="time" id="r_c1" value="'+esc(r.c1)+'" oninput="rPrev()"><span>'+esc(t("alle"))+'</span><input type="time" id="r_c2" value="'+esc(r.c2)+'" oninput="rPrev()"></div>';
 else if(k=="day")h=dayBtns("r_cd",r.cd);
 $("r_cf").innerHTML=h;rPrev()}
function aDefault(k){return k=="color"?{k:k,v:"ff0000"}:k=="ledmode"?{k:k,v:"fixed"}:k=="bright"?{k:k,v:128}:k=="gpio"?{k:k,p:4,v:1}:k=="wait"?{k:k,v:5}:k=="note"?{k:k,v:""}:k=="air"?{k:k,v:"on boot"}:k=="mq"?{k:k,p:"evento",v:"ciao"}:{k:k}}
function aAdd(){RU.ed.acts.push(aDefault("color"));aDraw()}
function aDraw(){var h="";RU.ed.acts.forEach(function(a,i){h+='<div class="arow"><select class="ak" onchange="aKind('+i+',this.value)">'+selOpts(AKS,a.k)+'</select>';
  if(a.k=="color")h+='<input type="color" value="#'+esc(String(a.v).replace("#",""))+'" oninput="aSet('+i+',\'v\',this.value.replace(\'#\',\'\'))">';
  else if(a.k=="ledmode")h+='<select onchange="aSet('+i+',\'v\',this.value)">'+selOpts(LEDM,a.v)+'</select>';
  else if(a.k=="bright")h+='<input type="range" min="0" max="255" value="'+a.v+'" oninput="aSet('+i+',\'v\',+this.value)">';
  else if(a.k=="gpio")h+='<input type="number" min="0" max="48" value="'+a.p+'" oninput="aSet('+i+',\'p\',+this.value)" style="max-width:90px"><select onchange="aSet('+i+',\'v\',+this.value)">'+selOpts(ONOFF,a.v)+'</select>';
  else if(a.k=="wait")h+='<input type="number" min="1" max="3600" value="'+a.v+'" oninput="aSet('+i+',\'v\',+this.value)" style="max-width:110px"><span>'+esc(t("secondi"))+'</span>';
  else if(a.k=="mq")h+='<input maxlength="40" value="'+esc(a.p)+'" placeholder="'+esc(t("argomento"))+'" oninput="aSet('+i+',\'p\',this.value)" style="max-width:130px"><input maxlength="60" value="'+esc(a.v)+'" placeholder="'+esc(t("testo"))+'" oninput="aSet('+i+',\'v\',this.value)">';
  else if(a.k=="air")h+='<select onchange="aSet('+i+',\'v\',this.value)">'+selOpts(AIRO,a.v)+'</select>';
  else if(a.k=="note")h+='<input maxlength="60" value="'+esc(a.v)+'" placeholder="'+esc(t("Testo della nota"))+'" oninput="aSet('+i+',\'v\',this.value)">';
  h+='<button class="ib" onclick="aMove('+i+',-1)" aria-label="'+esc(t("Su"))+'">&uarr;</button><button class="ib" onclick="aMove('+i+',1)" aria-label="'+esc(t("Giu"))+'">&darr;</button><button class="ib" onclick="aDel('+i+')" aria-label="'+esc(t("Elimina"))+'">&#10005;</button></div>'});
 $("racts").innerHTML=h;rPrev()}
function aKind(i,k){RU.ed.acts[i]=aDefault(k);aDraw()}
function aSet(i,f,v){RU.ed.acts[i][f]=v;rPrev()}
function aMove(i,d){var a=RU.ed.acts,j=i+d;if(j<0||j>=a.length)return;var x=a[i];a[i]=a[j];a[j]=x;aDraw()}
function aDel(i){RU.ed.acts.splice(i,1);aDraw()}
function rCollect(){var r=RU.ed;r.name=rClean($("r_name").value);r.tk=$("r_tk").value;
 if(r.tk=="time"){r.h=$("r_h").value||"07:00";r.d=dayMask("r_d")}
 else if(r.tk=="every"||r.tk=="after")r.n=Math.max(1,Math.round((+$("r_n").value||1)*(+$("r_u").value)));
 else if(r.tk=="temp")r.tmp=+$("r_tmp").value||70;
 r.ck=$("r_ck").value;if(r.ck=="between"){r.c1=$("r_c1").value||"22:00";r.c2=$("r_c2").value||"06:00"}else if(r.ck=="day")r.cd=dayMask("r_cd");return r}
function rPrev(){if(!RU.ed||!$("r_tk")||!$("rprev"))return;var r=rCollect();$("rprev").textContent=rSum(r)}
function rCancel(){RU.ed=null;$("redit").className="card rcard hide"}
function rSave(){var r=rCollect(),e="";
 if(!r.name)e=t("Scrivi un nome");else if(!r.acts.length)e=t("Aggiungi almeno un'azione");else if(r.tk=="time"&&r.d=="0000000")e=t("Scegli almeno un giorno");else if(r.ck=="day"&&r.cd=="0000000")e=t("Scegli almeno un giorno");
 else if((r.tk=="every")&&r.n<5)e=t("Il timer ripetuto parte da 5 secondi");
 if(e){msg("rem",e,false);return}
 if(RU.edi<0)RU.list.push(r);else RU.list[RU.edi]=r;
 var txt=RU.list.map(rSer).join("\n");
 post("/api/rules",{text:txt}).then(function(x){return x.json()}).then(function(j){if(j.ok){RU.ed=null;$("redit").className="card rcard hide";msg("rm",t("Regola salvata"),true);rLoad()}else{if(RU.edi<0)RU.list.pop();msg("rem",j.err||t("Errore"),false)}})}

