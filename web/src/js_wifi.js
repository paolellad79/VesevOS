/* ===== Rete: Wi-Fi e indirizzo IP; modo aereo (si mostra in Dispositivi > Interfacce > Radio) ===== */
var RN={c:null,n:null,s:"wifi"};
function airUi(){
  var v=$("air_ex").value;
  $("air_t").className=v=="1"?"agrid":"agrid hide";
  $("air_h").className=v=="2"?"":"hide";
  var n=RN.n||{};
  $("airst").innerHTML=n.air?'<span class="pill a">'+esc(t("Attivo"))+'</span>'
    :'<span class="pill">'+esc(t("Spento"))+'</span>'
}

function airOn(){
  var ex=+$("air_ex").value,par=0;
  if(ex==1){
    par=Math.round(+$("air_n").value*+$("air_u").value);
    if(!(par>=10)){
      msg("airm",t("Tempo minimo 10 secondi"),false);
      return
    }
  }
  if(ex==2){
    var h=$("air_at").value;
    if(!h){
      msg("airm",t("Scegli l'orario"),false);
      return
    }
    par=+h.slice(0,2)*60+ +h.slice(3,5)
  }
  var how=ex==0?t("al prossimo avvio"):ex==1?tf("dopo {0}",durText(par)):ex==2
    ?tf("alle {0}",$("air_at").value):t("solo a mano");
  dlg({title:t("Attivare il modo aereo?"),
    body:'<p style="margin:0">'+
    esc(t("Tutte le radio si spengono subito e questa pagina non si raggiunge piu."))+
    '</p><p style="margin:8px 0 0">'+esc(tf("Il modo aereo finisce {0}.",how))+
    '</p><p style="margin:8px 0 0;color:var(--mut)">'+
    esc(t("Per uscire prima: airplane off dalla seriale, oppure tasto fisico se la sua azione non e stata cambiata."))+'</p>',ok:t("Attiva"),
    danger:true,
    onOk:function(){
      return post("/api/airplane",
        {on:1,exit:ex,param:par}).then(function(r){return r.json()}).then(function(j){
          if(j.ok)setTimeout(function(){
            dlg({title:t("Modo aereo attivo"),
              body:'<p style="margin:0">'+esc(tf("Il modo aereo finisce {0}. Poi ricarica la pagina.",how))+
              '</p>',buttons:[["ok",t("Ricarica"),""]],onOk:function(){
                location.reload();
                return""
              }})
          },50);
          return j.ok?"":j.err
        })
    }})
}

function ipv(id,v,on){
  var e=$(id);
  e.value=v||"";
  e.disabled=!on
}

function ipFill(ch){
  var n=RN.n,c=RN.c,ap=n.st==1,dh=$("w_dhcp").value=="1",ids=["w_ip","w_mask","w_gw","w_d1","w_d2"],
    v;
  $("w_dhcp").disabled=ap;
  $("ipsave").disabled=ap;
  if(ap){
    v=[n.ip,n.mask,n.gw,"",""];
    $("ipnote").textContent=
      t(
        "Punto di accesso attivo: indirizzo fisso, non modificabile. Per cambiarlo collega la scheda a una rete (scheda Wi-Fi).")
  }
  else if(dh){
    v=n.st==3?[n.ip,n.mask,n.gw,n.dns,n.dns2]:["","","","",""];
    $("ipnote").textContent=n.st==3?t("Valori assegnati dal router (DHCP): non modificabili.")
      :t("Non connesso: nessun indirizzo assegnato.")
  }
  else{
    v=(ch&&n.st==3&&(c.staDhcp||!c.ip))?[n.ip,n.mask,n.gw,n.dns,n.dns2]
      :[c.ip,c.mask,c.gw,c.dns1,c.dns2];
    $("ipnote").textContent=t("Indirizzo fisso: scrivi i valori.")
  }
  ids.forEach(function(id,i){ipv(id,v[i],!ap&&!dh)});
  $("ipmac").innerHTML=macRow(t("MAC client"),n.mac)+macRow(t("MAC punto di accesso"),n.apMac)
}

function dh(){ipFill(true)}
function fillNet(){
  Promise.all([api("/api/settings").then(function(r){return r.json()}),
    api("/api/status").then(function(r){return r.json()})]).then(function(a){
      RN.c=a[0];
      RN.n=a[1].net;
      $("w_ssid").value=RN.c.staSsid;
      $("w_dhcp").value=RN.c.staDhcp?"1":"0";
      ipFill(false);
      drawAp()
    }).catch(function(){})
}

function saveWifi(){
  var mid=cur=="ip"?"wm2":"wm";
  if(!$("w_ssid").value){
    msg(mid,t("Prima scegli la rete in Rete > Wi-Fi"),false);
    return
  }
  post("/api/wifi/save",{ssid:$("w_ssid").value,pass:$("w_pass").value,dhcp:$("w_dhcp").value,
    ip:$("w_ip").value,mask:$("w_mask").value,gw:$("w_gw").value,d1:$("w_d1").value,
    d2:$("w_d2").value})
    .then(function(r){return r.json()}).then(function(j){
      msg(mid,j.ok?t("Salvato. La scheda si sta collegando: se cambia indirizzo riconnettiti.")
        :j.err,j.ok);
      if(j.ok)setTimeout(fillNet,9000)
    }).catch(function(){})
}

function apOnly(){
  post("/api/wifi/ap",{}).then(function(r){return r.json()}).then(function(j){
    msg("wm3",j.ok?t("Modo AP attivo (192.168.4.1)"):j.err,j.ok);
    if(j.ok)setTimeout(fillNet,5000)
  }).catch(function(){})
}

function scan(){
  $("scanmsg").textContent=t("Ricerca in corso...");
  api("/api/wifi/scan",{method:"POST"}).then(function(){setTimeout(scanRes,3000)})
}

function scanRes(){
  api("/api/wifi/scan").then(function(r){return r.json()}).then(function(j){
    if(j.running){
      setTimeout(scanRes,1500);
      return
    }
    $("scanmsg").textContent=tf("{0} reti trovate",j.list.length);
    var h="<tr><th>"+t("Rete")+"</th><th>"+t("Segnale")+"</th><th>"+t("Canale")+
      "</th><th></th></tr>";
    j.list.forEach(function(n,i){
      h+="<tr><td>"+esc(n.ssid)+(n.open?" ("+t("aperta")+")":"")+"</td><td>"+n.rssi+" dBm</td><td>"+
        n.ch+'</td><td><button class="btn gray" onclick="pick('+i+')">'+t("Scegli")+
        '</button></td></tr>'
    });
    $("scanlist").innerHTML=h;
    window._sc=j.list
  })
}

function pick(i){
  $("w_ssid").value=window._sc[i].ssid;
  $("w_pass").focus()
}
