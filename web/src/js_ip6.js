/* Pagina Rete > IP > IPv6: elenco degli indirizzi, interruttore e indirizzo fisso (rotta /api/ip6). */
var IP6={};

function apriIp(){
  fillNet();
  ip6Load()
}

function ip6Tipo(n){
  return [t("locale"),t("globale"),t("privato"),t("altro")][n]||""
}

function ip6Modo(){
  $("i6_fx").classList.toggle("hide",$("i6_modo").value!="1")
}

function ip6Draw(j){
  var h="",st=$("i6_stato");
  IP6=j;
  if(!j.presente)st.textContent=t("IPv6 non e disponibile in questo firmware.");
  else if(!j.acceso)st.textContent=t("IPv6 e spento: accendilo dalle impostazioni qui sotto.");
  else if(!j.collegato)st.textContent=t("La scheda non e collegata alla Wi-Fi di casa: gli indirizzi compaiono dopo il collegamento.");
  else if(!j.elenco.length)st.textContent=t("IPv6 acceso: attendo gli indirizzi dal router.");
  else st.textContent=tf("Indirizzi IPv6: {0}",j.elenco.length);
  if(j.elenco.length)h="<tr><th>"+t("Indirizzo")+"</th><th>"+t("Tipo")+"</th><th>"+t("Stato")+"</th></tr>";
  j.elenco.forEach(function(a){
    h+="<tr><td>"+esc(a.a)+"/"+j.prefisso+"</td><td>"+esc(ip6Tipo(a.t))+"</td><td>"+
      esc(a.p?t("in uso"):t("in verifica"))+"</td></tr>"
  });
  $("i6_tab").innerHTML=h;
  $("i6_on").classList.toggle("on",!!j.acceso);
  $("i6_modo").value=j.fisso?"1":"0";
  if(document.activeElement!==$("i6_ind"))$("i6_ind").value=j.ind||"";
  if(document.activeElement!==$("i6_gw"))$("i6_gw").value=j.gw||"";
  if(document.activeElement!==$("i6_dns"))$("i6_dns").value=j.dns||"";
  ip6Modo()
}

function ip6Load(){
  api("/api/ip6").then(function(r){return r.json()}).then(ip6Draw).catch(function(){})
}

function ip6Save(){
  post("/api/ip6",{acceso:$("i6_on").classList.contains("on")?"1":"0",
    fisso:$("i6_modo").value,ind:$("i6_ind").value.trim(),gw:$("i6_gw").value.trim(),
    dns:$("i6_dns").value.trim()})
    .then(function(r){return r.json()}).then(function(j){
      if(j.err){
        msg("i6m",j.err,false);
        return
      }
      msg("i6m",t("Salvato"),true);
      ip6Draw(j);
      setTimeout(ip6Load,3000)
    }).catch(function(){})
}
