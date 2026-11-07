/* ===== App (.vab): elenco, pagina generica con i controlli dichiarati, avvio e stop. Il testo si modifica con l'editor dei file ===== */
var AP={list:[],elf:[],prg:{},dat:{sd:false,on:{}},sel:"",info:null,tm:0,run:false,busy:false,
  key:"",tmr:0};
function apLoad(){
  apTimerStop();
  apLists().then(function(){
    if(AP.sel)apPoll();
    apRunPoll();
    AP.tm=setInterval(function(){
      if(cur!="app"||document.hidden){
        if(cur!="app")apTimerStop();
        return
      }
      if(AP.sel)apPoll();
      apRunPoll()
    },1500)
  })
}

function apTimerStop(){
  if(AP.tm){
    clearInterval(AP.tm);
    AP.tm=0
  }
}
/* elenchi: app Berry (/api/apps), app ELF installate (/api/elf, se il firmware le ha) e cio che gira (/api/progs) */
function apLists(){
  var a=api("/api/apps").then(function(r){return r.json()}).then(function(j){
    if(!j.ok){
      msg("appm",j.err,false);
      return
    }
    AP.list=j.list
  }).catch(function(){}),
    e=AUTH&&AUTH.elf?api("/api/elf").then(function(r){return r.json()}).then(function(j){AP.elf=
        j.mods||[]}).catch(function(){}):Promise.resolve(),
    d=api("/api/progdata").then(function(r){return r.json()}).then(function(j){
      var m={};
      (j.list||[]).forEach(function(x){m[x.k+":"+x.n]=1});
      AP.dat={sd:!!j.sd,on:m}
    }).catch(function(){});
  return Promise.all([a,e,d,apRunPoll()]).then(function(){
    AP._h="";
    apDraw()
  })
}

function apRunPoll(){
  return api("/api/progs").then(function(r){return r.json()}).then(function(j){
    var m={};
    (j.progs||[]).forEach(function(p){if(p.a)m[p.k+":"+p.n]=p});
    AP.prg=m;
    apDraw()
  }).catch(function(){})
}

function apIco(x){
  if(x.ok&&x.icon){
    var sv=x.icon;
    if(sv.indexOf("xmlns")<0)sv=sv.replace("<svg",'<svg xmlns="http://www.w3.org/2000/svg"');
    return'<img class="api" alt="" width="24" height="24" src="data:image/svg+xml;charset=utf-8,'+
      enc(sv)+'">'
  }
  return'<svg class="i" aria-hidden="true"><use href="#i-fvap"/></svg>'
}
/* una riga per programma, uguale per tutti: nome, tag dell'estensione, stato, permessi che chiede e gli stessi pulsanti (Avvia, Ferma, Elimina) */
function apRow(k,x){
  var n=x.n,q=esc(n),oq="'"+k+"','"+n+"'",run=AP.prg&&AP.prg[k+":"+n],valid=x.ok!==false,
    adm=ROLE==2,ext=progExt(k).slice(1),nm=/^[a-z0-9_-]{1,24}$/.test(n),
    name='<b>'+(k=="b"&&valid?esc(x.t||n):q)+'</b>',
    use=k=="b"?(x.u||""):(x.u||"").replace(/ /g,","),
    st=!valid?'<span class="pill k">'+esc(t("errore"))+'</span>':x.q
    ?'<span class="pill k">'+esc(t("in quarantena"))+'</span>':run
    ?'<span class="pill a">'+esc(t("in corso"))+'<span data-ps="'+k+':'+n+'"></span></span>'
    :'<span class="pill">'+esc(t("pronto"))+'</span>',
    h='<tr><td><span class="scn">'+
    (k=="b"?apIco(x):'<svg class="i" aria-hidden="true"><use href="#i-fvap"/></svg>')+name+
    '<span class="pill">'+esc(ext)+'</span>'+st+'</span>';
  if(!valid)h+='<div class="fhint bad">'+esc(x.e)+'</div>';
  else{
    var parts=[];
    if(k=="b")parts.push(n,tf("Versione {0}",x.v));
    if(k=="b"&&x.a)parts.push(x.a);
    if(k=="b")parts.push(tf("Controlli: {0}",x.c));
    parts.push(tf("Permessi: {0}",use?use.replace(/,/g,", "):"-"));
    h+='<div class="fhint">'+esc(parts.join(" · "))+'</div>';
    if(x.d)h+=apDataSel(k,n,run);
    if(x.m)h+='<div class="fhint bad">'+
      esc(tf("Chiede {0}, che e spenta (Sistema > Funzionalita)",x.m))+'</div>'
  }
  var can=valid&&!x.q&&(k=="e"?adm:true);
  h+='</td><td class="fsz">'+(valid&&adm?fSize(k=="b"?x.s:x.b):"")+
    '</td><td class="fac tkb"><span class="svi">'+
    (k=="b"&&valid&&nm?sibB("sm go","i-fvap",tf("Apri {0}",n),AP.sel==n,false,"apOpen('"+n+"')")
      :"")+
    (valid?sibB("sm go","i-sgo",tf("Avvia {0}",n),!!run,!can||!!run,can&&!run?"apGo("+oq+")":"")+
      sibB("sm st","i-sst",tf("Ferma {0}",n),false,!run||!adm,run&&adm?"apStopN('"+n+"')":""):"")+
    (adm&&k=="e"&&x.q?sibB("sm","i-lockx",tf("Togli {0} dalla quarantena",n),false,false,
      "apUnq('"+n+"')"):"")+
    (adm&&k=="b"&&nm?sibB("sm","i-edit",tf("Modifica {0}",n),false,false,"apEdit('"+n+"')"):"")+
    (adm?sibB("sm","i-del",tf("Elimina {0}",n),false,!!run,run?"":"apDel("+oq+")"):"")+
    '</span></td></tr>';
  return h
}
/* dove l'app salva i dati (solo se dichiara di salvarne): lo sceglie l'utente; senza microSD montata resta solo la flash */
function apDataSel(k,n,run){
  var sd=AP.dat.sd,on=!!AP.dat.on[k+":"+n],id="pd_"+k+"_"+n,dis=ROLE!=2||!!run?" disabled":"";
  return'<div class="fhint"><label for="'+id+'">'+esc(t("Salva su"))+'</label> <select id="'+id+'"'+
    dis+' onchange="apData(\''+k+'\',\''+n+'\',this.value)"><option value="flash"'+
    (on?"":" selected")+'>'+esc(t("Flash"))+'</option>'+
    (sd||on?'<option value="sd"'+(on?" selected":"")+'>'+
      esc(on&&!sd?t("microSD (non montata)"):t("microSD"))+'</option>':"")+'</select></div>'
}

function apData(k,n,v){
  post("/api/progdata",{k:k,name:n,where:v}).then(function(r){return r.json()}).then(function(j){
    msg("appm",j.ok===false?j.err:t("Fatto"),j.ok!==false);
    apLists()
  }).catch(function(){apLists()})
}

function apDraw(){
  var h="",bn=AP.list||[],el=AP.elf||[];
  if(!bn.length&&!el.length)h=
    '<tbody><tr><td colspan="3" class="fhint">'+
    esc(ROLE==2?t("Nessuna app. Premi Nuova app per crearne una, oppure Carica.")
      :t("Nessuna app disponibile per il tuo ruolo."))+'</td></tr></tbody>';
  else{
    h="<tbody>";
    bn.forEach(function(x){h+=apRow("b",x)});
    el.forEach(function(x){h+=apRow("e",x)});
    h+="</tbody>"
  }
  if(AP._h!==h){
    AP._h=h;
    $("aptab").innerHTML=h
  }
  Object.keys(AP.prg||{}).forEach(function(key){
    var e=document.querySelector('[data-ps="'+key+'"]');
    if(e)e.textContent=" \u00b7 "+AP.prg[key].s+" s"
  })
}   /* i secondi si aggiornano senza ridisegnare la riga (i pulsanti tengono il fuoco) */

function apGo(k,n){
  if(k=="b"){
    post("/api/app/start",{name:n}).then(function(r){return r.json()}).then(function(j){
      msg("appm",j.ok?t("App avviata"):j.err,j.ok);
      apRunPoll();
      if(AP.sel==n)apPoll()
    }).catch(function(){})
  }
  else post("/api/progs",{act:"start",k:k,
    name:n}).then(function(r){return r.json()}).then(function(j){
      msg("appm",j.ok?t("App avviata"):j.err,j.ok);
      apRunPoll()
    }).catch(function(){})
}

function apStopN(n){
  post("/api/progs",{act:"stop",name:n}).then(function(r){return r.json()}).then(function(j){
    msg("appm",j.ok?t("App fermata"):j.err,j.ok);
    apRunPoll();
    if(AP.sel==n)apPoll()
  }).catch(function(){})
}

function apUnq(n){
  post("/api/elf",{act:"unq",name:n}).then(function(r){return r.json()}).then(function(j){
    msg("appm",j.ok===false?j.err:t("Fatto"),j.ok!==false);
    apLists()
  }).catch(function(){})
}
/* Carica: un solo pulsante, il tipo si riconosce dall'estensione */
function apUp(){
  var f=$("apup").files[0];
  if(!f)return;
  var ok=extOk(f.name,"b")||(AUTH&&AUTH.elf&&extOk(f.name,"e"));
  if(!ok){
    msg("appm",tf("Il file deve finire per {0}",
      AUTH&&AUTH.elf?EXT.b+" "+t("oppure")+" "+EXT.e:EXT.b),false);
    return
  }
  var fd=new FormData();
  fd.append("file",f);
  api("/api/fs/up?dir=/bin",
    {method:"POST",body:fd}).then(function(r){return r.json()}).then(function(j){
      msgCaricato("appm",j,t("Caricato"));
      $("apup").value="";
      apLists()
    }).catch(function(){})
}

function apPath(n){return"/bin/"+n+EXT.b}
function apEdit(n){
  progTry("/bin/",n,"b",function(p){
    return api("/api/fs/text?path="+enc(p)).then(function(r){
      return r.text().then(function(x){return{ok:r.ok,x:x,p:p}})
    })
  }).then(function(o){
    if(!o.ok){
      msg("appm",o.x,false);
      return
    }
    fEdOpen(o.p,o.x,false)
  }).catch(function(){})
}

function apTpl(n){
  return"[manifest]\nnome="+n+"\ntitolo="+n.charAt(0).toUpperCase()+n.slice(1)+
    "\nversione=1\npermessi=led\nchi=admin\n\n[icon]\n<svg viewBox=\"0 0 24 24\"><path d=\"M5 19l4-9 3 4 3-6 4 11z\" fill=\"none\" stroke=\"#c0392b\" stroke-width=\"2\"/></svg>\n\n[page]\nswitch id=power label=\"Accesa\"\nslider id=lum label=\"Luminosita\" min=0 max=255\ntext id=info label=\"Stato\"\n\n[code]\ndef on_set(id, v)\n  if id == 'power'\n    if v == 1\n      led('ffffff')\n    else\n      led('auto')\n    end\n  end\n  if id == 'lum'\n    dev_set('info', 'Luminosita ' + str(v))\n  end\nend\n\n[data]\npower=0\n"
}

function apNew(){
  dlg({title:t("Nuova app"),
    body:'<label for="dn">'+esc(t("Nome dell'app"))+
    '</label><input id="dn" autocomplete="off" maxlength="24" spellcheck="false" autocapitalize="off" placeholder="lampada"><p class="fhint" style="margin:0">'+
    esc(t("Da 1 a 24 caratteri: lettere minuscole, numeri, trattino e sottotrattino."))+'</p>',
    ok:t("Crea"),
    onOk:function(){
      var n=dlgV("dn").toLowerCase().replace(/\.va[bp]$/,"");
      if(!/^[a-z0-9_-]{1,24}$/.test(n))return t("Nome non valido");
      if(AP.list.some(function(x){return x.n==n}))return t("Esiste gia");
      fEdOpen(apPath(n),apTpl(n),true);
      return""
    }})
}

function apDel(k,n){
  if(n===undefined){
    n=k;
    k="b"
  }
  dlg({title:t("Eliminare?"),
    body:'<p style="margin:0">'+esc(tf("L'app {0} verra eliminata.",n))+
    '</p><p style="margin:8px 0 0;color:var(--mut)">'+esc(t("Non si puo annullare."))+'</p>',
    ok:t("Elimina"),danger:true,
    onOk:function(){
      if(AP.sel==n)apClose();
      return progTry("/bin/",n,k,function(p){
        return post("/api/fs/del",{path:p}).then(function(r){return r.json()})
      }).then(function(j){
        apLists();
        return j.ok?"":j.err
      })
    }})
}
/* ---- la pagina generica dell'app: UN solo disegno nel firmware, legge i controlli dichiarati nel manifest (niente HTML dell'utente) ---- */
function apOpen(n){
  api("/api/app?name="+enc(n)).then(function(r){return r.json()}).then(function(j){
    if(!j.ok){
      msg("appm",j.err,false);
      return
    }
    AP.sel=n;
    AP.info=j;
    AP.key="";
    AP.run=false;
    apBuild();
    apPoll();
    AP._h="";
    apDraw();
    $("apcard").scrollIntoView({block:"nearest"})
  }).catch(function(){})
}

function apClose(){
  AP.sel="";
  AP.info=null;
  AP.key="";
  $("apcard").className="card hide";
  AP._h="";
  apDraw({list:AP.list})
}

function apCtl(c){
  var id=esc(c.id),l=esc(c.l),dis=AP.run?"":" disabled",x='<div class="apc" id="apw_'+id+'">';
  if(c.k=="switch")x+='<span class="apl" id="apt_'+id+'">'+l+
    '</span><button type="button" class="sw1" id="apc_'+id+
    '" role="switch" aria-checked="false" aria-labelledby="apt_'+id+'"'+dis+' onclick="apTog(\''+
    c.id+'\')"></button>';
  else if(c.k=="slider")x+='<label class="apl" for="apc_'+id+'">'+l+
    '</label><input type="range" id="apc_'+id+'" min="'+(+c.mn)+'" max="'+(+c.mx)+'" value="'+
    (+c.mn)+'"'+dis+' oninput="apSl(\''+c.id+'\',this.value,0)" onchange="apSl(\''+c.id+
    '\',this.value,1)"><output class="apv" id="apo_'+id+'"></output>';
  else if(c.k=="color")x+='<label class="apl" for="apc_'+id+'">'+l+
    '</label><input type="color" id="apc_'+id+'" value="#000000"'+dis+' onchange="apSend(\''+c.id+
    '\',this.value.slice(1))">';
  else if(c.k=="text")x+='<label class="apl" for="apc_'+id+'">'+l+
    '</label><input type="text" id="apc_'+id+'" maxlength="40"'+dis+
    ' onkeydown="if(event.key===\'Enter\')apSend(\''+c.id+'\',this.value)" onchange="apSend(\''+
    c.id+'\',this.value)">';
  else if(c.k=="button")x+='<button type="button" class="btn gray" id="apc_'+id+'"'+dis+
    ' onclick="apSend(\''+c.id+'\',1)">'+l+'</button>';
  else x+='<span class="apl" id="apt_'+id+'">'+l+'</span><svg class="apg" id="apc_'+id+
    '" viewBox="0 0 200 60" preserveAspectRatio="none" role="img" aria-labelledby="apt_'+id+
    '"></svg><output class="apv" id="apo_'+id+'"></output>';
  return x+'</div>'
}

function apBuild(){
  var j=AP.info;
  if(!j)return;
  $("apcard").className="card";
  $("aphead").innerHTML=apIco({ok:true,icon:j.icon})+'<b>'+esc(j.t)+'</b>';
  $("apctl").innerHTML=j.ctl.length?j.ctl.map(apCtl).join("")
    :'<p class="fhint">'+esc(t("Questa app non ha controlli: lavora da sola."))+'</p>'
}

function apState(j){
  AP.run=!!j.run;
  AP.busy=!!j.busy;
  var k=(AP.run?1:0)+"|"+(AP.busy?1:0);
  $("appill").className="pill"+(AP.run?" a":"");
  $("appill").textContent=AP.run?t("in esecuzione"):t("ferma");
  $("apgo").disabled=AP.busy;
  $("apst").disabled=!AP.run;
  $("apgo").setAttribute("aria-pressed",AP.run?"true":"false");
  if(AP.key!==k){
    AP.key=k;
    [].forEach.call($("apctl").querySelectorAll("input,button"),function(e){e.disabled=!AP.run})
  }
    (j.vals||[]).forEach(function(v){
      var e=$("apc_"+v.id);
      if(!e)return;
      var f=document.activeElement===e;
      if(v.n!==undefined){
        if(e.getAttribute("role")=="switch"){
          e.className="sw1"+(v.n?" on":"");
          e.setAttribute("aria-checked",v.n?"true":"false")
        }else if(e.type=="range"){
          if(!f)e.value=v.n;
          var o=$("apo_"+v.id);
          if(o)o.textContent=v.n
        }
      }
      else if(v.a!==undefined)apChart(e,v.a,$("apo_"+v.id));
      else if(v.s!==undefined&&!f){
        if(e.type=="color")e.value="#"+(/^[0-9a-f]{6}$/.test(v.s)?v.s:"000000");
        else if(e.type=="text")e.value=v.s
      }
    });
  var o=$("apout");
  o.textContent=(j.out||"")+(j.err?(j.out?"\n":"")+j.err:"");
  o.className="lgv"+(j.err?" bad":"")+(o.textContent?"":" hide")
}

function apChart(svg,a,out){
  var n=a.length,mn=1e18,mx=-1e18,p="",i;
  if(out)out.textContent=n?a[n-1]:"";
  if(!n){
    svg.innerHTML="";
    return
  }
  for(i=0;i<n;i++){
    if(a[i]<mn)mn=a[i];
    if(a[i]>mx)mx=a[i]
  }
  if(mx==mn){mx=mn+1}
  for(i=0;i<n;i++)p+=(n>1?i*200/(n-1):100).toFixed(1)+","+(56-(a[i]-mn)*52/(mx-mn)).toFixed(1)+" ";
  svg.innerHTML='<polyline points="'+p+
    '" fill="none" stroke="currentColor" stroke-width="2" vector-effect="non-scaling-stroke"/>'
}

function apPoll(){
  if(!AP.sel)return;
  var n=AP.sel;
  api("/api/app/state?name="+enc(n)).then(function(r){return r.json()}).then(function(j){
    if(n!=AP.sel)return;
    if(!j.ok){
      msg("appm",j.err,false);
      apClose();
      return
    }
    apState(j)
  }).catch(function(){})
}

function apStart(){
  post("/api/app/start",{name:AP.sel}).then(function(r){return r.json()}).then(function(j){
    msg("appm",j.ok?t("App avviata"):j.err,j.ok);
    apPoll()
  }).catch(function(){})
}

function apStop(){
  post("/api/app/stop",{name:AP.sel}).then(function(r){return r.json()}).then(function(j){
    msg("appm",j.ok?t("App fermata"):j.err,j.ok);
    apPoll()
  }).catch(function(){})
}

function apSend(id,v){
  if(!AP.run)return;
  post("/api/app/set",{name:AP.sel,id:id,v:v}).then(function(r){return r.json()}).then(function(j){
    if(!j.ok)msg("appm",j.err,false);
    else apPoll()
  }).catch(function(){})
}

function apTog(id){
  var e=$("apc_"+id);
  apSend(id,e&&e.getAttribute("aria-checked")=="true"?0:1)
}

function apSl(id,v,fin){
  var o=$("apo_"+id);
  if(o)o.textContent=v;
  if(fin){
    clearTimeout(AP.tmr);
    apSend(id,v);
    return
  }
  if(AP.tmr)return;
  AP.tmr=setTimeout(function(){
    AP.tmr=0;
    var e=$("apc_"+id);
    if(e)apSend(id,e.value)
  },250)
}
