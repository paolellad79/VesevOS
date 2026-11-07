/* ===== periferiche a classi (rele, led, pin, ingresso, analogico): API /api/io, schede modulo /api/mods ===== */
var IOCN={rele:"Rele",led:"LED",pin:"Pin (uscita)",ingresso:"Ingresso",
  analogico:"Analogico (pin 1-10)",sd:"Lettore microSD"},IOS={sel:"",tm:0,list:[],sdp:[]};
function ioCn(c){return c=="i2c"?t("Sensore I2C (BMP280)"):t(IOCN[c]||c)}
function ioStop(){
  if(IOS.tm){
    clearInterval(IOS.tm);
    IOS.tm=0
  }
}

function ioIco(svg){
  if(!svg)return'<svg class="i" aria-hidden="true"><use href="#i-chipn"/></svg>';
  if(svg.indexOf("xmlns")<0)svg=svg.replace("<svg",'<svg xmlns="http://www.w3.org/2000/svg"');
  return'<img class="api" alt="" width="24" height="24" src="data:image/svg+xml;charset=utf-8,'+
    enc(svg)+'">'
}

function ioFind(n){
  for(var i=0;i<IOS.list.length;i++)if(IOS.list[i].n==n)return IOS.list[i];
  return null
}

function ioNum(x,dg){
  return x.toLocaleString(undefined,{minimumFractionDigits:dg,maximumFractionDigits:dg})
}

function ioI2cVal(d){
  return d.ok?ioNum(d.v/100,1)+" °C · "+ioNum(d.p/100,1)+" hPa":t("non risponde")
}

function ioPins(d){
  return d.c=="i2c"?"SDA "+d.g+" · SCL "+d.g2:d.c=="sd"
    ?"CS "+d.g+" · MOSI "+d.g2+" · SCK "+d.g3+" · MISO "+d.g4:String(d.g)
}

function ioCanSw(d){return d.out&&(ROLE>=2||(ROLE>=1&&d.c!="rele"))}
function ioLoad(){
  var sel=IOS.sel;
  $("io_new").className=ROLE>=2?"card":"card hide";
  api("/api/io").then(function(r){return r.json()}).then(function(j){
    IOS.list=j.io||[];
    IOS.sdp=j.sdp||[];
    if(sel&&!ioFind(sel)){
      ioClose();
      return
    }
    if(sel){
      ioDetDraw();
      return
    }
    $("io_main").className="";
    $("io_det").className="card hide";
    ioStop();
    ioListDraw();
  }).catch(function(){})
}

function ioListDraw(){
  var L=IOS.list,h="";
  if(!L.length){
    $("io_tab").innerHTML='<tr><td class="fhint">'+esc(t("Nessuna voce"))+'</td></tr>';
    return
  }
  h='<tr><th>'+esc(t("Nome"))+'</th><th>'+esc(t("Classe"))+'</th><th>GPIO</th><th>'+
    esc(t("Valore"))+'</th><th></th></tr>';
  L.forEach(function(d){
    var v;
    if(ioCanSw(d))v='<button class="sw1'+(d.v?" on":"")+'" role="switch" aria-checked="'+
      (d.v?"true":"false")+'" aria-label="'+esc(d.n)+'" onclick="ioSw(\''+d.n+'\','+(d.v?0:1)+
      ')"></button>';
    else if(d.c=="analogico")v='<b>'+d.v+'</b>';
    else if(d.c=="i2c")v='<b>'+esc(ioI2cVal(d))+'</b>';
    else if(d.c=="sd")v='<span class="pill dvp '+(d.v?"v":"")+'">'+
      esc(d.v?t("Scheda montata"):t("Scheda non montata"))+'</span>';
    else v='<span class="pill dvp '+(d.v?"v":"")+'">'+esc(d.v?t("acceso"):t("spento"))+'</span>';
    h+='<tr><td><b>'+esc(d.n)+'</b></td><td>'+esc(ioCn(d.c))+
      ((d.f&1)?' <span class="fhint">('+esc(t("attivo basso"))+')</span>':"")+'</td><td>'+
      esc(ioPins(d))+'</td><td>'+v+'</td><td><button class="btn gray" onclick="ioOpen(\''+d.n+
      '\')">'+esc(t("Gestisci"))+'</button></td></tr>'
  });
  $("io_tab").innerHTML=h
}
/* --- pagina di gestione di una periferica --- */
function ioOpen(n){
  IOS.sel=n;
  $("io_main").className="hide";
  $("io_det").className="card";
  ioDetDraw();
  ioStop();
  IOS.tm=setInterval(ioPoll,1000)
}

function ioClose(){
  IOS.sel="";
  ioStop();
  $("io_det").className="card hide";
  $("io_main").className="";
  ioLoad()
}

function ioPoll(){
  if(cur!="io"||!IOS.sel){
    ioStop();
    return
  }
  api("/api/io").then(function(r){return r.json()}).then(function(j){
    IOS.list=j.io||[];
    if(IOS.sel&&!ioFind(IOS.sel)){
      ioClose();
      return
    }
    ioDetLive()
  }).catch(function(){})
}

function ioDetDraw(){
  var d=ioFind(IOS.sel);
  if(!d){
    ioClose();
    return
  }
  var adm=ROLE>=2,sw=ioCanSw(d),h='';
  h+='<div class="aphd"><h3 style="margin:0">'+esc(d.n)+
    '</h3><button class="btn gray" onclick="ioClose()">'+esc(t("Indietro"))+'</button></div>';
  h+='<table class="ftab" style="margin-top:8px"><tr><td>'+esc(t("Classe"))+'</td><td><b>'+
    esc(ioCn(d.c))+'</b></td></tr><tr><td>GPIO</td><td><b>'+esc(ioPins(d))+'</b></td></tr>';
  if(d.c=="i2c")h+='<tr><td>'+esc(t("Indirizzo"))+'</td><td><b>0x'+d.a.toString(16)+'</b> · '+
    esc(d.d)+((d.f&8)?' · 400 kHz':' · 100 kHz')+'</td></tr>';
  if(d.c=="sd")h+='<tr><td>'+esc(t("Frequenza del bus SPI"))+'</td><td><b>'+d.k+
    ' kHz</b></td></tr>';
  if(d.out||d.c=="ingresso")h+='<tr><td>'+esc(t("Livello"))+'</td><td><b>'+
    esc((d.f&1)?t("attivo basso"):t("attivo alto"))+'</b></td></tr>';
  if(d.c=="ingresso")h+='<tr><td>'+esc(t("Resistenza"))+'</td><td><b>'+
    esc((d.f&2)?"pull-up":(d.f&4)?"pull-down":t("nessuna"))+'</b></td></tr>';
  h+='<tr><td>'+esc(t("Valore"))+'</td><td id="iod_v"></td></tr></table>';
  h+='<h4 style="margin:14px 0 6px">'+esc(t("Prova"))+'</h4>';
  if(d.out){
    h+='<div class="ftb">';
    if(sw){
      h+='<button class="btn" onclick="ioAct(\'set\',1)">'+esc(t("Accendi"))+
        '</button><button class="btn gray" onclick="ioAct(\'set\',0)">'+esc(t("Spegni"))+
        '</button><button class="btn gray" onclick="ioAct(\'pulse\',1000)">'+
        esc(t("Impulso di 1 secondo"))+'</button>';
      if(d.c!="rele")h+='<button class="btn gray" onclick="ioAct(\'blink\',5)">'+
        esc(t("Lampeggia per 5 secondi"))+'</button>'
    }
    else h+='<span class="fhint">'+esc(t("Il rele lo prova solo l'Amministratore."))+'</span>';
    h+='</div><p class="fhint">'+
      esc(d.c=="rele"?t(
        "Il rele si riaccende al massimo una volta al secondo e non lampeggia: si consuma. Spegnere si puo sempre.")
        :t("L'impulso e il lampeggio si spengono da soli."))+'</p>'
  }
  else if(d.c=="i2c")h+='<p class="fhint">'+
    esc(t(
      "Temperatura e pressione si aggiornano da sole ogni due secondi. Se il sensore non risponde, controlla i fili, l'indirizzo (0x76 o 0x77) e l'alimentazione."))+
    '</p>';
  else if(d.c=="sd")h+='<p class="fhint">'+
    esc(t(
      "La scheda si monta e si smonta dal riquadro Scheda microSD, sopra nella pagina Periferiche. Se non la trova, controlla i fili, che sia FAT32 e prova una frequenza piu bassa (400 kHz)."))+
    '</p>';
  else if(d.c=="ingresso")h+='<p class="fhint">'+
    esc(t("Premi il pulsante o attiva il sensore: il valore qui sotto cambia da solo."))+'</p>';
  else h+='<p class="fhint">'+esc(t("Il valore va da 0 a 4095 e si aggiorna da solo."))+'</p>';
  h+='<div id="iod_m" class="msg" role="status"></div>';
  if(adm)h+='<div class="ftb" style="margin-top:14px"><button class="btn gray" onclick="ioRen(\''+
    d.n+'\')">'+esc(t("Rinomina"))+'</button><button class="btn red" onclick="ioDel(\''+d.n+'\')">'+
    esc(t("Togli"))+'</button></div>';
  if(d.c!="sd")h+='<p class="fhint">'+
    esc(d.c=="i2c"?tf(
      "Nelle app: use={0} e poi dev_get(\"{0}\") per la temperatura (centesimi di grado) e dev_get(\"{0}.p\") per la pressione (pascal).",
      d.n):tf("Nelle app: use={0} e poi dev_set(\"{0}\", 1) o dev_get(\"{0}\").",d.n))+'</p>';
  $("io_det").innerHTML=h;
  ioDetLive()
}

function ioDetLive(){
  var d=ioFind(IOS.sel),e=$("iod_v");
  if(!d||!e)return;
  if(d.c=="i2c")e.innerHTML='<b>'+esc(ioI2cVal(d))+'</b>';
  else if(d.c=="sd")e.innerHTML=
    '<span class="pill dvp '+(d.v?"v":"")+'">'+esc(d.v?t("Scheda montata"):t("Scheda non montata"))+
    '</span>';
  else if(d.c=="analogico")e.innerHTML=
    '<b>'+d.v+
    '</b> <span class="fhint">/ 4095</span><div class="bar" style="margin-top:4px"><i style="width:'+
    Math.round(d.v*100/4095)+'%"></i></div>';
  else e.innerHTML='<span class="pill dvp '+(d.v?"v":"")+'">'+esc(d.v?t("acceso"):t("spento"))+
    '</span>'
}

function ioAct(a,v){
  post("/api/dev/act",{id:IOS.sel,a:a,arg:v}).then(function(r){return r.json()}).then(function(j){
    if(j.ok===false)msg("iod_m",j.err,false);
    else msg("iod_m",t("Fatto"),true);
    ioPoll()
  }).catch(function(){})
}
/* --- driver vdb: l'elenco e il caricamento stanno nella pagina Driver (js_driver.js); qui il dialogo di installazione e l'eliminazione --- */
function ioModOpen(n){
  api("/api/mod?name="+enc(n)).then(function(r){return r.json()}).then(function(j){
    if(!j.ok){
      msg("drvm",j.err,false);
      return
    }
    var b='';
    b+=modSchema(j);
    if(j.wiring){
      var sv=j.wiring;
      if(sv.indexOf("xmlns")<0)sv=sv.replace("<svg",'<svg xmlns="http://www.w3.org/2000/svg"');
      b+='<details style="margin-top:8px"><summary>'+esc(t("Disegno allegato al file"))+
        '</summary><img alt="'+esc(tf("Schema del modulo {0}",j.t))+
        '" style="width:100%;max-width:420px;background:#fff;border-radius:8px" src="data:image/svg+xml;charset=utf-8,'+
        enc(sv)+'"></details>'
    }
    if(j.p=="230V")b+='<p class="fhint" style="color:var(--kot)"><b>'+
      esc(t(
        "Rete elettrica 230 V: serve un modulo a norma e un installatore. VesevOS comanda solo il segnale."))+
      '</b></p>';
    if(j.w)b+='<p class="fhint"><b>'+esc(j.w)+'</b></p>';
    if(!j.free)b+='<p class="fhint" style="color:var(--kot)">'+
      esc(t("Non ci sono abbastanza pin liberi per questo modulo"))+'</p>';
    if(j.app)b+='<p class="fhint">'+
      esc(tf(
        "Questo driver porta una app con pagina e codice. Permessi dell'app: {0}. La usa solo l'Admin.",
        j.u||"-"))+'</p>';
    b+='<p class="fhint" style="margin-bottom:2px"><b>'+esc(t("Pin"))+'</b> '+
      esc(t(
        "La scheda propone questi pin liberi. Puoi cambiarli: il sistema controlla che siano ammessi e non gia usati."))+
      '</p>';
    j.sig.forEach(function(s,i){
      b+='<label for="io_pn'+i+'">'+esc(s)+'</label><input id="io_pn'+i+
        '" inputmode="numeric" maxlength="2" autocomplete="off" style="width:5em" value="'+
        (j.sug[i]!=null?j.sug[i]:"")+'">'
    });
    b+='<label for="io_pf">'+esc(t("Nome (facoltativo)"))+
      '</label><input id="io_pf" maxlength="9" autocomplete="off" spellcheck="false" autocapitalize="off">';
    dlg({title:j.t,body:b,ok:t("Installa"),onOk:function(){
      var pn=j.sig.map(function(s,i){return dlgV("io_pn"+i)});
      if(!pn.every(function(x){
        return /^[0-9]{1,2}$/.test(x)
      }))return t("Indica un pin per ogni segnale");
      return post("/api/mod/install",
        {name:n,prefix:dlgV("io_pf"),
          pins:pn.join(",")}).then(function(r){return r.json()}).then(function(k){
            if(!k.ok)return k.err;
            var dv=k.devs||[],same=dv.length>1&&dv[0].n==dv[1].n;
            msg("drvm",t("Fatto")+": "+(same?dv[0].n+" ("+dv.map(function(d){
              return d.s+" = GPIO"+d.g
            }).join(", ")+")":dv.map(function(d){return d.n+" = GPIO"+d.g}).join(", ")),true);
            ioLoad();
            return""
          })
    }})
  }).catch(function(){})
}

function ioModDel(n){
  dlg({title:t("Eliminare?"),
    body:'<p style="margin:0">'+
    esc(tf("Il driver {0} verra eliminato. Le periferiche gia create restano.",n))+'</p>',
    ok:t("Elimina"),danger:true,
    onOk:function(){
      return post("/api/mod/del",{n:n,name:n}).then(function(r){return r.json()}).then(function(j){
        if(j.ok){
          drvLoad();
          return""
        }
        return j.err
      })
    }})
}

function ioSw(n,v){
  post("/api/dev/act",{id:n,a:"set",arg:v}).then(function(r){return r.json()}).then(function(j){
    if(j.ok===false)msg("io_m",j.err,false);
    ioLoad()
  }).catch(function(){})
}
/* funzioni possibili del pin scritto: la classe Analogico vale solo sui pin 1-10 (ADC1); le altre classi vanno su ogni pin ammesso.
   Il controllo vero dei pin (vietati, usati, doppi) resta del firmware: qui si toglie solo cio che il pin non sa fare. */
function ioPinFn(){
  var g=parseInt($("io_g").value),f=$("io_fn"),
    an=$("io_c").querySelector('option[value="analogico"]'),ok=!(g>0)||(g>=1&&g<=10);
  an.disabled=!ok;
  if(!ok&&$("io_c").value=="analogico"){
    $("io_c").value="ingresso";
    ioClsChg()
  }
  f.textContent=g>0&&typeof PFN!="undefined"&&PFN[g]
    ?tf("Funzioni possibili: {0}",PFN[g].replace(/ /g,", ")):""
}

function ioClsChg(){
  var s=$("io_c").value=="sd";
  $("io_sdf").className=s?"":"hide";
  $("io_o").parentNode.className=s?"hide":"";
  if(s){
    var D=IOS.sdp,F=["io_g","io_sm","io_sk","io_si"];
    F.forEach(function(i,k){if(!$(i).value&&D[k]!=null)$(i).value=D[k]});
    if(!$("io_sf").value)$("io_sf").value=1000
  }
}

function ioAdd(){
  var g=$("io_g").value,sd=$("io_c").value=="sd",
    o=sd?"mosi="+$("io_sm").value+" sck="+$("io_sk").value+" miso="+$("io_si").value+" khz="+
    $("io_sf").value:$("io_o").value;
  post("/api/io/add",{cls:$("io_c").value,gpio:g,name:$("io_n").value,
    opt:o}).then(function(r){return r.json()}).then(function(j){
      msg("io_am",j.ok?t("Fatto")+": "+j.n:j.err,!!j.ok);
      if(j.ok){
        $("io_n").value="";
        $("io_g").value="";
        ioLoad()
      }
    }).catch(function(){})
}

function ioDel(n){
  dlg({title:t("Togliere la periferica?"),body:'<p style="margin:0">'+esc(n)+'</p>',ok:t("Togli"),
    danger:true,onOk:function(){
      return post("/api/io/del",{n:n}).then(function(r){return r.json()}).then(function(j){
        if(j.ok){
          ioLoad();
          return""
        }
        return j.err
      })
    }})
}

function ioRen(n){
  dlg({title:t("Rinomina"),
    body:'<label for="dn">'+esc(t("Nuovo nome"))+
    '</label><input id="dn" autocomplete="off" maxlength="11" value="'+esc(n)+'">',ok:t("Rinomina"),
    onOk:function(){
      var to=dlgV("dn");
      return post("/api/io/rename",{n:n,to:to}).then(function(r){return r.json()}).then(function(j){
        if(j.ok){
          if(IOS.sel==n)IOS.sel=to;
          ioLoad();
          return""
        }
        return j.err
      })
    }})
}
