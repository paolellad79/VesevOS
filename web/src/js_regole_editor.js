/* ===== Automazioni: finestra di modifica ===== */
function selOpts(list,cur){
  return list.map(function(o){
    return'<option value="'+o[0]+'"'+(o[0]==cur?" selected":"")+'>'+esc(t(o[1]))+'</option>'
  }).join("")
}

function dayBtns(id,m){
  var h='<div class="dsel" id="'+id+'">';
  for(var i=0;i<7;i++)h+='<button type="button" class="'+(m[i]=="1"?"on":"")+
    '" onclick="this.classList.toggle(\'on\');rPrev()">'+esc(t(DAYS[i][1]))+'</button>';
  return h+'</div>'
}

function dayMask(id){
  var o="";
  var b=$(id).children;
  for(var i=0;i<7;i++)o+=b[i].classList.contains("on")?"1":"0";
  return o
}
var TKS=[["time","Orario (sveglia)"],["every","Ogni tanto (timer ripetuto)"],
  ["after","Dopo l'avvio (una volta)"],["boot","All'avvio"],["wifiup","Quando il Wi-Fi si collega"],
  ["wifidown","Quando il Wi-Fi si scollega"],["temp","Quando la temperatura supera"]];
var CKS=[["-","Sempre"],["between","Solo in una fascia oraria"],["day","Solo in certi giorni"]];
var AIRO=[["on boot","attivo fino al prossimo avvio"],["on 10m","attivo per 10 minuti"],
  ["on 1h","attivo per 1 ora"],["on fisso","attivo, si spegne solo a mano"],
  ["off","spento (rete accesa)"]];
var AKS=[["color","LED: colore"],["ledmode","LED: modo"],["bright","LED: luminosita"],
  ["gpio","Pin: acceso o spento"],["wait","Aspetta"],["note","Scrivi nel registro"],
  ["air","Modo aereo"],["mq","MQTT: invia un messaggio"],["ntp","Aggiorna l'ora"],
  ["script","Avvia uno script"],["app","Avvia un'app"],["appstop","Ferma l'app"],
  ["reboot","Riavvia la scheda"]];
function unitOf(n){return n%3600==0?3600:n%60==0?60:1}
function rScrNames(){
  if(ROLE<2){
    RU.scn=null;
    RU.apn=null;
    return
  }
  api("/api/apps").then(function(r){return r.json()}).then(function(j){
    RU.apn=j.ok?j.list.filter(function(x){return x.ok}).map(function(x){return x.n}):null;
    if(RU.ed&&$("racts"))aDraw()
  }).catch(function(){});
  api("/api/script").then(function(r){return r.json()}).then(function(j){
    RU.scn=j.ok?j.list.map(function(x){return x.n}):null;
    if(RU.ed&&$("racts"))aDraw()
  }).catch(function(){})
}

function rEditDraw(){
  var r=RU.ed;
  rScrNames();
  $("redit").innerHTML='<h3>'+esc(RU.edi<0?t("Nuova regola"):t("Modifica regola"))+'</h3>'+
    '<label for="r_name">'+esc(t("Nome"))+'</label><input id="r_name" maxlength="24" value="'+
    esc(r.name)+'" oninput="rPrev()">'+
    '<h4>'+esc(t("Quando"))+'</h4><select id="r_tk" onchange="rTrigUi()">'+selOpts(TKS,r.tk)+
    '</select><div id="r_tf"></div>'+
    '<h4>'+esc(t("Se (facoltativo)"))+'</h4><select id="r_ck" onchange="rCondUi()">'+
    selOpts(CKS,r.ck)+'</select><div id="r_cf"></div>'+
    '<h4>'+esc(t("Allora"))+'</h4><div id="racts"></div><button class="btn gray" onclick="aAdd()">'+
    esc(t("Aggiungi azione"))+'</button>'+
    '<p id="rprev" class="subt" style="margin-top:12px"></p>'+
    '<button class="btn" onclick="rSave()">'+esc(t("Salva"))+
    '</button><button class="btn gray" onclick="rCancel()">'+esc(t("Annulla"))+
    '</button><div id="rem" class="msg"></div>';
  rTrigUi();
  rCondUi();
  aDraw();
  $("redit").className="card rcard";
  $("redit").scrollIntoView({behavior:"smooth",block:"start"})
}

function rTrigUi(){
  var r=RU.ed,k=$("r_tk").value,h="";
  r.tk=k;
  if(k=="time")h='<label for="r_h">'+esc(t("Ora"))+'</label><input type="time" id="r_h" value="'+
    esc(r.h)+'" oninput="rPrev()"><label>'+esc(t("Giorni"))+'</label>'+dayBtns("r_d",r.d);
  else if(k=="every"||k=="after"){
    var u=unitOf(r.n);
    h='<label>'+esc(t("Tempo"))+
      '</label><div class="arow"><input type="number" id="r_n" min="1" value="'+(r.n/u)+
      '" oninput="rPrev()"><select id="r_u" onchange="rPrev()">'+selOpts(UNITS,u)+'</select></div>'
  }
  else if(k=="temp")h='<label for="r_tmp">'+esc(t("Temperatura (gradi C)"))+
    '</label><input type="number" id="r_tmp" min="30" max="110" value="'+r.tmp+
    '" oninput="rPrev()">';
  $("r_tf").innerHTML=h;
  rPrev()
}

function rCondUi(){
  var r=RU.ed,k=$("r_ck").value,h="";
  r.ck=k;
  if(k=="between")h='<div class="arow"><span>'+esc(t("Dalle"))+
    '</span><input type="time" id="r_c1" value="'+esc(r.c1)+'" oninput="rPrev()"><span>'+
    esc(t("alle"))+'</span><input type="time" id="r_c2" value="'+esc(r.c2)+
    '" oninput="rPrev()"></div>';
  else if(k=="day")h=dayBtns("r_cd",r.cd);
  $("r_cf").innerHTML=h;
  rPrev()
}

function aDefault(k){
  return k=="color"?{k:k,v:"ff0000"}:k=="ledmode"?{k:k,v:"fixed"}:k=="bright"?{k:k,v:128}:k=="gpio"
    ?{k:k,p:4,v:1}:k=="wait"?{k:k,v:5}:k=="note"?{k:k,v:""}:k=="air"?{k:k,v:"on boot"}:k=="script"
    ?{k:k,v:""}:k=="app"?{k:k,v:""}:k=="mq"?{k:k,p:"evento",v:"ciao"}:{k:k}
}

function aAdd(){
  RU.ed.acts.push(aDefault("color"));
  aDraw()
}

function aDraw(){
  var h="";
  RU.ed.acts.forEach(function(a,i){
    h+='<div class="arow"><select class="ak" onchange="aKind('+i+',this.value)">'+selOpts(AKS,a.k)+
      '</select>';
    if(a.k=="color")h+='<input type="color" value="#'+esc(String(a.v).replace("#",""))+
      '" oninput="aSet('+i+',\'v\',this.value.replace(\'#\',\'\'))">';
    else if(a.k=="ledmode")h+='<select onchange="aSet('+i+',\'v\',this.value)">'+selOpts(LEDM,a.v)+
      '</select>';
    else if(a.k=="bright")h+='<input type="range" min="0" max="255" value="'+a.v+'" oninput="aSet('+
      i+',\'v\',+this.value)">';
    else if(a.k=="gpio")h+='<input type="number" min="0" max="'+brdMax()+'" value="'+a.p+
      '" oninput="aSet('+i+',\'p\',+this.value)" style="max-width:90px"><select onchange="aSet('+i+
      ',\'v\',+this.value)">'+selOpts(ONOFF,a.v)+'</select>';
    else if(a.k=="wait")h+='<input type="number" min="1" max="3600" value="'+a.v+'" oninput="aSet('+
      i+',\'v\',+this.value)" style="max-width:110px"><span>'+esc(t("secondi"))+'</span>';
    else if(a.k=="mq")h+='<input maxlength="40" value="'+esc(a.p)+'" placeholder="'+
      esc(t("argomento"))+'" oninput="aSet('+i+
      ',\'p\',this.value)" style="max-width:130px"><input maxlength="60" value="'+esc(a.v)+
      '" placeholder="'+esc(t("testo"))+'" oninput="aSet('+i+',\'v\',this.value)">';
    else if(a.k=="air")h+='<select onchange="aSet('+i+',\'v\',this.value)">'+selOpts(AIRO,a.v)+
      '</select>';
    else if(a.k=="script"){
      if(RU.scn&&RU.scn.length){
        if(!a.v)a.v=RU.scn[0];
        var sn=RU.scn.slice();
        if(a.v&&sn.indexOf(a.v)<0)sn.push(a.v);
        h+='<select aria-label="'+esc(t("nome dello script"))+'" onchange="aSet('+i+
          ',\'v\',this.value)">'+sn.map(function(n){
            return'<option value="'+esc(n)+'"'+(n==a.v?" selected":"")+'>'+
              esc(RU.scn.indexOf(n)<0?tf("{0} (non trovato)",n):n)+'</option>'
          }).join("")+'</select>'
      }
      else h+='<input maxlength="24" value="'+esc(a.v)+'" placeholder="'+
        esc(t("nome dello script"))+'" aria-label="'+esc(t("nome dello script"))+'" oninput="aSet('+
        i+',\'v\',this.value)">'
    }
    else if(a.k=="app"){
      if(RU.apn&&RU.apn.length){
        if(!a.v)a.v=RU.apn[0];
        var an=RU.apn.slice();
        if(a.v&&an.indexOf(a.v)<0)an.push(a.v);
        h+='<select aria-label="'+esc(t("nome dell'app"))+'" onchange="aSet('+i+
          ',\'v\',this.value)">'+an.map(function(n){
            return'<option value="'+esc(n)+'"'+(n==a.v?" selected":"")+'>'+
              esc(RU.apn.indexOf(n)<0?tf("{0} (non trovato)",n):n)+'</option>'
          }).join("")+'</select>'
      }
      else h+='<input maxlength="24" value="'+esc(a.v)+'" placeholder="'+esc(t("nome dell'app"))+
        '" aria-label="'+esc(t("nome dell'app"))+'" oninput="aSet('+i+',\'v\',this.value)">'
    }
    else if(a.k=="note")h+='<input maxlength="60" value="'+esc(a.v)+'" placeholder="'+
      esc(t("Testo della nota"))+'" oninput="aSet('+i+',\'v\',this.value)">';
    h+='<button class="ib" onclick="aMove('+i+',-1)" aria-label="'+esc(t("Su"))+
      '">&uarr;</button><button class="ib" onclick="aMove('+i+',1)" aria-label="'+esc(t("Giu"))+
      '">&darr;</button><button class="ib" onclick="aDel('+i+')" aria-label="'+esc(t("Elimina"))+
      '">&#10005;</button></div>'
  });
  $("racts").innerHTML=h;
  rPrev()
}

function aKind(i,k){
  RU.ed.acts[i]=aDefault(k);
  aDraw()
}

function aSet(i,f,v){
  RU.ed.acts[i][f]=v;
  rPrev()
}

function aMove(i,d){
  var a=RU.ed.acts,j=i+d;
  if(j<0||j>=a.length)return;
  var x=a[i];
  a[i]=a[j];
  a[j]=x;
  aDraw()
}

function aDel(i){
  RU.ed.acts.splice(i,1);
  aDraw()
}

function rCollect(){
  var r=RU.ed;
  r.name=rClean($("r_name").value);
  r.tk=$("r_tk").value;
  if(r.tk=="time"){
    r.h=$("r_h").value||"07:00";
    r.d=dayMask("r_d")
  }
  else if(r.tk=="every"||r.tk=="after")r.n=
    Math.max(1,Math.round((+$("r_n").value||1)*(+$("r_u").value)));
  else if(r.tk=="temp")r.tmp=+$("r_tmp").value||70;
  r.ck=$("r_ck").value;
  if(r.ck=="between"){
    r.c1=$("r_c1").value||"22:00";
    r.c2=$("r_c2").value||"06:00"
  }else if(r.ck=="day")r.cd=dayMask("r_cd");
  return r
}

function rPrev(){
  if(!RU.ed||!$("r_tk")||!$("rprev"))return;
  var r=rCollect();
  $("rprev").textContent=rSum(r)
}

function rCancel(){
  RU.ed=null;
  $("redit").className="card rcard hide"
}

function rSave(){
  var r=rCollect(),e="";
  if(!r.name)e=t("Scrivi un nome");
  else if(!r.acts.length)e=t("Aggiungi almeno un'azione");
  else if(r.tk=="time"&&r.d=="0000000")e=t("Scegli almeno un giorno");
  else if(r.ck=="day"&&r.cd=="0000000")e=t("Scegli almeno un giorno");
  else if((r.tk=="every")&&r.n<5)e=t("Il timer ripetuto parte da 5 secondi");
  if(e){
    msg("rem",e,false);
    return
  }
  if(RU.edi<0)RU.list.push(r);
  else RU.list[RU.edi]=r;
  var txt=RU.list.map(rSer).join("\n");
  post("/api/rules",{text:txt}).then(function(x){return x.json()}).then(function(j){
    if(j.ok){
      RU.ed=null;
      $("redit").className="card rcard hide";
      msg("rm",t("Regola salvata"),true);
      rLoad()
    }else{
      if(RU.edi<0)RU.list.pop();
      msg("rem",j.err||t("Errore"),false)
    }
  })
}
