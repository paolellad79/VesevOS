/* ===== Programmi in corso e sottosistemi (pagina Task) + Funzionalita (Sistema) ===== */
/* Sottosistemi: vdb (script, app e comandi dei driver) e vde (app e driver in codice nativo). L'interruttore ferma tutti i loro programmi. */
var PRG={subs:[],progs:[],_k:""};
function subHint(id){
  return id=="vdb"?t("Script, app e comandi dei driver."):t("App e driver in codice nativo.")
}
SVT.vdb={set:function(v){return subSet("vdb",v)}};
SVT.vde={set:function(v){return subSet("vde",v)}};
function subSet(id,v){
  return post("/api/sub",{id:id,on:v?1:0}).then(function(r){return r.json()}).then(function(j){
    if(j.ok===false)msg("tkm",j.err,false);
    else if(typeof tkPoll=="function")tkPoll()
  })
}

function progExt(k){return k=="s"?EXT.s:k=="e"?EXT.e:EXT.b}
function progHow(p){
  return p.a?t("in corso"):p.h==1?t("fermato"):p.h==2?t("scaduto"):tf("finito, valore {0}",p.rc)
}
/* chiamata da tkPoll con la risposta di /api/tasklist: righe dei sottosistemi e dei programmi */
function tkProgs(j){
  PRG.subs=j.subs||[];
  PRG.progs=j.progs||[];
  var key=JSON.stringify([PRG.subs,PRG.progs,ROLE]),box=$("tksub"),pg=$("tkprog");
  if(!box||!pg)return;
  if(PRG._k===key)return;
  PRG._k=key;
  var h="";
  PRG.subs.forEach(function(s){
    h+='<div class="row"><span><b>'+esc(s.id=="vdb"?t("Sottosistema vdb"):t("Sottosistema vde"))+
      '</b><br><span class="fhint">'+
      esc(subHint(s.id)+" "+tf("In corso: {0}, memoria {1}.",s.n,fSize(s.m)))+
      '</span></span><span class="sc" id="sc_'+s.id+'"></span></div>'
  });
  box.innerHTML=h;
  PRG.subs.forEach(function(s){svcSt(s.id,s.on,false,s.on)});
  var p="";
  if(!PRG.progs.length)p='<p class="fhint">'+esc(t("Nessun programma in esecuzione."))+'</p>';
  else{
    p='<table class="ftab"><thead><tr><th>'+esc(t("Nome"))+'</th><th>'+esc(t("Stato"))+'</th><th>'+
      esc(t("Secondi"))+'</th><th>'+esc(t("Memoria"))+'</th><th></th></tr></thead><tbody>';
    PRG.progs.forEach(function(x){
      var oq="'"+x.k+"','"+x.n+"'";
      p+='<tr><td><b>'+esc(x.n)+'</b> <span class="pill">'+esc(progExt(x.k).slice(1))+'</span>'+
        (x.u?'<div class="fhint">'+esc(tf("Permessi: {0}",x.u.replace(/ /g,", ")))+'</div>':"")+
        '</td><td>'+esc(progHow(x))+'</td><td>'+x.s+'</td><td>'+esc(fSize(x.m))+
        '</td><td class="fac tkb">'+
        (ROLE==2?'<span class="svi">'+
          sibB("sm st","i-sst",tf("Ferma {0}",x.n),false,!x.a,x.a?"progAct('stop',"+oq+")":"")+
          sibB("sm rs","i-srs",tf("Riavvia {0}",x.n),false,false,"progAct('restart',"+oq+")")+
          '</span>':"")+'</td></tr>'
    });
    p+='</tbody></table>'
  }
  pg.innerHTML=p
}

function progAct(a,k,n){
  post("/api/progs",{act:a,k:k,name:n}).then(function(r){return r.json()}).then(function(j){
    msg("tkm",j.ok===false?j.err
      :(a=="stop"?tf("Programma '{0}' fermato",n):tf("Programma '{0}' avviato",n)),j.ok!==false);
    PRG._k="";
    setTimeout(function(){if(typeof tkPoll=="function")tkPoll()},400)
  }).catch(function(){})
}
/* ===== Funzionalita: i gruppi di funzioni che l'utente puo togliere a tutti i programmi ===== */
var FN={list:[]};
function fnRole(r){return r>=2?t("Admin"):r==1?t("Operatore"):t("Tutti")}
function fnLoad(){
  api("/api/func").then(function(r){return r.json()}).then(function(j){
    FN.list=j.list||[];
    fnDraw()
  }).catch(function(){})
}

function fnDraw(){
  var h="";
  FN.list.forEach(function(f){
    var lab=esc(f.id),dis=ROLE==2?"":" disabled";
    h+='<div class="row"><span><b>'+lab+'</b><br><span class="fhint">'+esc(f.d)+
      '</span><br><span class="fhint">'+
      esc(tf("Lettura: {0}. Scrittura e comandi: {1}. In uso ora: {2}.",fnRole(f.rr),fnRole(f.rw),
        f.u))+'</span></span>'+
      '<button type="button" class="sw1'+(f.on?" on":"")+'" role="switch" aria-checked="'+
      (f.on?"true":"false")+'" aria-label="'+esc(tf("Funzionalita {0}",f.id))+'"'+dis+
      ' onclick="fnSet(\''+f.id+'\','+(f.on?0:1)+')"></button></div>'
  });
  $("funclist").innerHTML=h
}

function fnSet(id,on){
  post("/api/func",{id:id,on:on}).then(function(r){return r.json()}).then(function(j){
    if(j.ok===false){
      msg("funcm",j.err,false);
      return
    }
    FN.list=j.list||FN.list;
    fnDraw();
    msg("funcm",on?tf("Funzionalita {0} concessa",id):tf("Funzionalita {0} tolta",id),true)
  }).catch(function(){})
}
