/* ===== Rete: punto di accesso e sottoschede ===== */
/* etichetta da stampare con nome rete, password e QR */
function apLabel(ss,pw){
  return'<div class="lab prt"><div class="qrbox">'+qrSvg(wifiQr(ss,pw))+
    '</div><div><div class="subt">'+esc(t("Rete Wi-Fi della scheda"))+'</div><div class="big">'+
    esc(ss)+'</div><div class="subt" style="margin-top:6px">'+esc(t("Password"))+
    '</div><div class="pw">'+esc(pw)+
    '</div><div class="subt" style="margin-top:6px">https://192.168.4.1</div></div></div>'
}
var APLAW="La password e casuale e diversa per ogni scheda: una password uguale per tutti aprirebbe tutte le schede, e le leggi sulla sicurezza dei dispositivi lo vietano (UE: direttiva radio con EN 18031 e Cyber Resilience Act; Regno Unito: PSTI). Conservala: stampa l'etichetta o fotografa il QR.";

/* ===== Rete > Punto di accesso ===== */
function drawAp(){
  var n=RN.n;
  $("apinfo").innerHTML=row(esc(t("Stato")),
    esc(n.st==1?t("Attivo"):t("Non attivo (sei collegato a una rete)")))+
    row(esc(t("Nome rete (SSID)")),esc(n.apSsid))+row(esc(t("Indirizzo IP")),"192.168.4.1")+
    row(esc(t("Subnet mask")),"255.255.255.0")+macRow(t("MAC punto di accesso"),n.apMac);
  if(!$("ap_ss").value)$("ap_ss").value=n.apSsid||"";
  if(ROLE>=2)apShow()
}

function apShow(){
  api("/api/ap").then(function(r){return r.json()}).then(function(j){
    $("apsec").innerHTML=apLabel(j.ssid,j.pass)+'<div class="law">'+esc(t(APLAW))+
      '</div><button class="btn gray" onclick="print()">'+esc(t("Stampa etichetta"))+'</button>'
  }).catch(function(){})
}

function apSave(nw){
  var ss=$("ap_ss").value.trim(),pw=$("ap_pw").value;
  if(!ss){
    msg("apm",t("Scrivi il nome della rete"),false);
    return
  }
  if(pw&&pw.length<8){
    msg("apm",t("Password Wi-Fi: da 8 a 63 caratteri"),false);
    return
  }
  dlg({title:t("Cambiare l'hotspot?"),
    body:'<p style="margin:0">'+
    esc(t("Chi e collegato all'hotspot viene scollegato e deve ricollegarsi con i dati nuovi."))+
    '</p>',ok:t("Cambia"),danger:true,
    onOk:function(){
      var d={ssid:ss};
      if(nw)d["new"]="1";
      else if(pw)d.pass=pw;
      return post("/api/ap",d).then(function(r){return r.json()}).then(function(j){
        if(j.ok){
          $("ap_pw").value="";
          $("apsec").innerHTML=apLabel(ss,j.pass)+'<div class="law">'+esc(t(APLAW))+'</div>';
          msg("apm",t("Salvato"),true)
        }
        return j.ok?"":j.err
      })
    }})
}
