/* ===== Menu in alto: riavvio, recovery, sleep ===== */
var AUTHSET=true;
function uMenu(e){
  e.stopPropagation();
  var m=$("um"),open=m.classList.contains("hide");
  $("thm").className="pop hide";
  m.className=open?"pop":"pop hide";
  if(open){
    var b=m.querySelector("button");
    if(b)b.focus()
  }
}

function umClose(){$("um").className="pop hide"}
document.addEventListener("click",function(e){
  var m=$("um");
  if(m&&!m.classList.contains("hide")&&!e.target.closest("#um"))umClose()
});

function uWait(txt,ms){
  dlg({title:txt,body:'<p style="margin:0">'+
    esc(t("La pagina si ricarica da sola quando la scheda e pronta."))+'</p>',buttons:[]});
  setTimeout(function(){location.reload()},ms)
}

function uReboot(){
  dlg({title:t("Riavviare la scheda?"),
    body:'<p style="margin:0">'+esc(t("La pagina resta irraggiungibile per qualche secondo."))+
    '</p>',ok:t("Riavvia"),onOk:function(){
      return post("/api/reboot",{}).then(function(r){return r.json()}).then(function(j){
        if(j.ok)setTimeout(function(){uWait(t("Riavvio in corso..."),10000)},50);
        return j.ok?"":j.err
      })
    }})
}

function uFw(){
  api("/api/recovery").then(function(r){return r.json()}).then(function(j){
    if(!j.present){
      dlg({title:t("Aggiornare il firmware?"),
        body:'<p style="margin:0">'+
        esc(t(
          "Il recovery non e presente su questa scheda: il firmware si aggiorna solo con il cavo USB."))+
        '</p>',buttons:[["ok",t("Chiudi"),""]]});
      return
    }
    dlg({title:t("Aggiornare il firmware?"),
      body:'<p style="margin:0">'+
      esc(t(
        "La scheda si riavvia nel recovery, una pagina separata dove carichi il file .bin di VesevOS. Servono utente e password di un amministratore."))+
      '</p>',ok:t("Riavvia nel recovery"),
      onOk:function(){
        return post("/api/recovery",{}).then(function(r){return r.json()}).then(function(j){
          if(j.ok)setTimeout(function(){uRecGo("http://"+(j.ip||"")+":80/",uRecName(j.host))},50);
          return j.ok?"":j.err
        })
      }})
  }).catch(function(){})
}

function uRecUp(){
  api("/api/recovery").then(function(r){return r.json()}).then(function(j){
    if(!j.present){
      dlg({title:t("Aggiornare il recovery?"),
        body:'<p style="margin:0">'+
        esc(t("Il recovery non e presente su questa scheda: si installa solo con il cavo USB."))+
        '</p>',buttons:[["ok",t("Chiudi"),""]]});
      return
    }
    dlg({title:t("Aggiornare il recovery?"),
      body:'<p style="margin:0">'+
      esc(tf("Recovery installato: {0}",j.ver||t("versione sconosciuta")))+
      '</p><p style="margin:8px 0">'+
      esc(tf(
        "Scegli il file .bin del recovery (al massimo {0} KB). La scheda lo controlla tutto prima di scriverlo: se e sbagliato il recovery attuale resta com'e.",
        j.kb))+'</p>'
      +'<label for="rcf">'+esc(t("File del recovery (.bin)"))+
      '</label><input type="file" id="rcf" accept=".bin">'
      +'<label for="rcs">'+esc(t("Impronta SHA-256 (facoltativa)"))+
      '</label><input id="rcs" autocomplete="off" spellcheck="false" maxlength="64">'
      +'<p class="law" style="margin:8px 0 0">'+
      esc(t(
        "Non spegnere la scheda durante l'aggiornamento (pochi secondi). VesevOS continua a funzionare."))+
      '</p>',ok:t("Aggiorna recovery"),
      onOk:function(){
        var f=$("rcf").files[0],h=$("rcs").value.trim();
        if(!f)return t("Scegli il file");
        if(f.size>j.kb*1024)return t("File troppo grande per lo spazio del recovery");
        if(h&&!/^[0-9a-fA-F]{64}$/.test(h))return t(
          "L'impronta SHA-256 deve avere 64 cifre esadecimali");
        var fd=new FormData();
        fd.append("file",f,f.name);
        msg("dlgm",t("Controllo e scrivo il recovery..."),true);
        return api("/api/recovery/up"+(h?"?sha="+enc(h):""),
          {method:"POST",body:fd}).then(function(r){return r.json()}).then(function(k){
            if(k.ok)setTimeout(function(){
              dlg({title:t("Recovery aggiornato"),
                body:'<p style="margin:0" role="status">'+
                esc(tf(
                  "Da {0} a {1}. VesevOS non si riavvia: il nuovo recovery si usa al prossimo aggiornamento del firmware.",
                  k.from||"?",k.to))+'</p>',buttons:[["ok",t("Chiudi"),""]]})
            },50);
            return k.ok?"":(k.err||t("Errore"))
          },function(){return t("Caricamento non riuscito")})
      }})
  }).catch(function(){})
}

function uRecPrb(x,ok,ko){
  var c=new AbortController(),m=setTimeout(function(){c.abort()},1500);
  fetch(x,{mode:"no-cors",cache:"no-store",signal:c.signal}).then(function(){
    clearTimeout(m);
    ok()
  },function(){
    clearTimeout(m);
    ko()
  })
}

function uRecOpen(u){location.href=u}
function uRecName(h){
  var l=location.hostname;
  if(!h||/^[0-9.]+$/.test(l)||l.indexOf(":")>=0)return"";
  return"http://"+(h.indexOf(".")>=0?h:h+".local")+"/"
}

function uRecGo(u,nm){
  var st=0,t0=Date.now(),t1=0,stop=false,sec=location.protocol==="https:";
  var go=function(){
    stop=true;
    uRecOpen(sec&&nm?nm:u)
  };   // da HTTPS non si puo sondare un http://: se si e arrivati col nome si apre quello (il nome gia si risolve per il browser), altrimenti l'IP
  dlg({title:t("Scheda nel recovery"),
    body:'<p style="margin:0" role="status">'+
    esc(t("Aspetto che la scheda riparta nel recovery: la pagina si apre da sola."))+'</p>',
    buttons:[["ok",t("Apri ora"),""]],onOk:function(){
      go();
      return""
    }});
  (function tick(){
    if(stop)return;
    if(Date.now()-t0>60000){
      stop=true;
      dlg({title:t("Scheda nel recovery"),
        body:'<p style="margin:0">'+esc(t("Non riesco ad aprirla da sola."))+
        '</p><p style="margin:8px 0 0">'+
        esc(tf(
          "Apri {0} in una finestra privata del browser (scrivi http, non https). Se in 30 secondi non trova la rete di casa, la scheda crea la rete Wi-Fi VesevOS-recovery: la password e sulla seriale.",
          u))+'</p><p style="margin:8px 0 0">'+
        esc(t("Dopo l'aggiornamento VesevOS riparte da solo."))+'</p>',
        buttons:[["ok",t("Apri il recovery"),""]],onOk:function(){
          uRecOpen(u);
          return""
        }});
      return
    }
    if(st===0)uRecPrb(location.origin+"/",function(){setTimeout(tick,1000)},function(){
      st=1;
      t1=Date.now();
      setTimeout(tick,1000)
    });
    else if(sec){
      if(Date.now()-t1>8000)go();
      else setTimeout(tick,1000)
    }
    else if(nm&&Date.now()-t1<6000)uRecPrb(nm,function(){
      stop=true;
      uRecOpen(nm)
    },function(){setTimeout(tick,1000)});
    else uRecPrb(u,go,function(){setTimeout(tick,1000)})
  })()
}

function uSleep(){
  dlg({title:t("Mettere la scheda in sleep?"),
    body:'<p style="margin:0">'+
    esc(t("La scheda entra in sonno profondo: Wi-Fi e LED spenti, consuma pochissimo."))+
    '</p><p style="margin:8px 0 0"><b>'+
    esc(t("Per riaccenderla premi il tasto RESET sulla scheda (o togli e ridai corrente)."))+
    '</b></p>',ok:t("Sleep"),danger:true,
    onOk:function(){
      return post("/api/sleep",{}).then(function(r){return r.json()}).then(function(j){
        if(j.ok)setTimeout(function(){
          dlg({title:t("Scheda in sleep"),
            body:'<p style="margin:0">'+
            esc(t("Premi RESET sulla scheda, poi ricarica questa pagina."))+'</p>',
            buttons:[["ok",t("Ricarica"),""]],onOk:function(){
              location.reload();
              return""
            }})
        },50);
        return j.ok?"":j.err
      })
    }})
}
