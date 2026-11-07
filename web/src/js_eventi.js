/* Pagina Eventi di sicurezza (solo Amministratore). I dati stanno nella RAM della scheda: nomi e indirizzi passano sempre da esc(). */
var EV={d:null};
function evNomi(){
  return[t("Password sbagliata"),t("Codice MFA sbagliato"),t("Indirizzo bloccato"),
    t("Rifiutato dal Filtro IP"),t("Troppe richieste"),t("Altro errore di accesso")]
}

function evNota(n){if(n)AL.push(["w",tf("Accessi bloccati da guardare: {0}",n),"evs"])}
function evQuando(x){
  if(x.q)return new Date(x.q*1000).toLocaleString();
  if(x.s<60)return t("meno di un minuto fa");
  return x.s<3600?tf("{0} minuti fa",Math.floor(x.s/60)):tf("{0} ore fa",Math.floor(x.s/3600))
}

function evFiltro(){
  var s=$("ev_tipo"),c=s.value,h='<option value="">'+esc(t("Tutti"))+'</option>';
  evNomi().forEach(function(n,i){h+='<option value="'+i+'">'+esc(n)+'</option>'});
  s.innerHTML=h;
  s.value=c
}

function evLoad(){
  api("/api/eventi").then(function(r){return r.json()}).then(function(d){
    if(cur!="evs")return;
    EV.d=d;
    evFiltro();
    evDraw()
  }).catch(function(){})
}

function evDraw(){
  var d=EV.d;
  if(!d)return;
  var f=$("ev_tipo").value,nomi=evNomi(),tot=d.tot.reduce(function(a,b){return a+b},0);
  $("ev_riass").innerHTML=
    (d.ora?row(t("Ultime 24 ore"),esc(tf("{0} eventi",d.ult24)))
      :row(t("Ultime 24 ore"),t("ora non impostata")))+
    row(t("Dall'ultimo riavvio"),esc(tf("{0} eventi",tot)));
  var l=d.lista.filter(function(x){return f===""||x.k==f}).sort(function(a,b){return a.s-b.s}),
    h="<caption>"+esc(t("Ultimi eventi, dal piu recente"))+"</caption>";
  if(!l.length){
    $("ev_tab").innerHTML=h+"<tr><td>"+esc(t("Nessun evento dall'ultimo riavvio."))+"</td></tr>";
    return
  }
  h+="<tr><th scope='col'>"+esc(t("Quando"))+"</th><th scope='col'>"+esc(t("Evento"))+
    "</th><th scope='col'>"+esc(t("Indirizzo"))+"</th><th scope='col'>"+esc(t("Canale"))+
    "</th><th scope='col'>"+esc(t("Utente"))+"</th><th scope='col'>"+esc(t("Volte"))+"</th></tr>";
  l.forEach(function(x){
    h+="<tr><td>"+esc(evQuando(x))+"</td><td>"+esc(nomi[x.k]||"")+"</td><td>"+esc(x.i||"-")+
      "</td><td>"+esc(x.c==1?t("seriale"):t("web"))+"</td><td>"+esc(x.u||"-")+"</td><td>"+x.n+
      "</td></tr>"
  });
  $("ev_tab").innerHTML=h
}

function evSvuota(){
  dlg({title:t("Svuotare gli eventi?"),
    body:'<p style="margin:0">'+
    esc(t("L'elenco e i contatori vengono cancellati. I blocchi attivi restano."))+'</p>',
    ok:t("Svuota"),
    onOk:function(){
      return post("/api/eventi/svuota",
        {confirm:"1"}).then(function(r){return r.json()}).then(function(j){
          if(j.ok)evLoad();
          return j.ok?"":j.err
        })
    }})
}
