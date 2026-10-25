/* Aggiornamento tramite recovery (solo Amministratore): due strade, file dal computer o rete (GitHub), e l'esito dell'ultimo aggiornamento.
   La scheda scrive un ordine, si riavvia nel recovery e il recovery esegue. Nessuna password passa al recovery. */
var UPD_CHK=0;
function updLastHtml(j){if(!j||!j.res)return"";
 return'<p style="margin:0 0 10px" role="status"><b>'+esc(t("Ultimo aggiornamento"))+':</b> '+esc(j.text||"")+(j.ver?' ('+esc(j.ver)+')':'')+(j.msg?'<br>'+esc(tf("Motivo indicato dal recovery: {0}",j.msg)):'')+'</p>'}
/* dopo l'accesso di un amministratore: se c'e un esito nuovo (la scheda e tornata da un aggiornamento) lo mostra una volta */
function updCheckNew(){if(UPD_CHK||ROLE!=2)return;UPD_CHK=1;
 api("/api/updorder").then(function(r){return r.json()}).then(function(j){if(!j.fresh||!j.res)return;
  dlg({title:j.res==1?t("Installazione completata"):t("Aggiornamento non riuscito"),body:updLastHtml(j),buttons:[["ok",t("Chiudi"),""]]})}).catch(function(){})}
function uRecNet(){api("/api/updorder").then(function(r){return r.json()}).then(function(j){
 if(!j.present){dlg({title:t("Aggiornare tramite recovery?"),body:'<p style="margin:0">'+esc(t("Il recovery non e presente su questa scheda: si installa solo con il cavo USB."))+'</p>',buttons:[["ok",t("Chiudi"),""]]});return}
 dlg({title:t("Aggiorna tramite recovery"),body:updLastHtml(j)+'<p style="margin:0 0 8px">'+esc(t("La scheda si riavvia nel recovery, che installa il nuovo firmware. Non serve inserire di nuovo la password. Scegli come dare il file:"))+'</p>',
  buttons:[["cancel",t("Annulla"),"gray"],["file",t("Carica un file dal computer"),""],["net",t("Aggiorna dalla rete (GitHub)"),""]],
  onOk:function(k){setTimeout(function(){if(k=="file")updFile(j);else updNet(j)},50);return""}})}).catch(function(){})}
/* il pulsante di conferma resta disattivo (disabled) finche la casella "Ho capito" non e spuntata */
function updGate(){var c=$("updc"),b=document.querySelector("#dlga button[data-k=ok]");if(!c||!b)return;
 function aggiorna(){b.disabled=!c.checked;b.setAttribute("aria-disabled",c.checked?"false":"true")}
 c.onchange=aggiorna;aggiorna()}
function updDlg(o){dlg(o);updGate()}
/* strada (a): file dal computer. Il browser passa da HTTPS a HTTP sulla rete locale: lo si dice e si chiede una conferma esplicita. */
function updFile(j){
 updDlg({title:t("Carica un file dal computer"),body:'<p style="margin:0 0 8px">'+esc(t("La scheda si riavvia nel recovery e questa pagina passa a un indirizzo http (senza cifratura) sulla tua rete locale, dove scegli il file .bin di VesevOS."))+'</p>'
  +'<p class="law" style="margin:0 0 8px">'+esc(t("Per non chiederti di nuovo la password, l'indirizzo contiene un codice monouso valido 15 minuti e legato a questo computer. Su una rete locale non fidata qualcuno potrebbe vederlo: usa questa strada solo a casa tua."))+'</p>'
  +'<label class="chk" for="updc"><input type="checkbox" id="updc"><span>'+esc(t("Ho capito: la pagina cambia e passa per HTTP"))+'</span></label>',ok:t("Riavvia e continua"),
  onOk:function(){if(!$("updc").checked)return t("Spunta la casella per continuare");
   return post("/api/updorder/file",{confirm:"1"}).then(function(r){return r.json()}).then(function(k){
    if(k.ok)setTimeout(function(){uRecGo(k.urlIp||k.url)},50);return k.ok?"":(k.err||t("Errore"))},function(){return t("Richiesta non riuscita")})}})}
/* strada (b): dalla rete. L'indirizzo e l'impronta si scrivono a mano (nessuna ricerca automatica); il recovery scarica e controlla. */
function updNet(j){
 updDlg({title:t("Aggiorna dalla rete (GitHub)"),body:'<p style="margin:0 0 8px">'+esc(t("Il recovery si collega alla rete di casa, chiede l'ora a un server pubblico (pool.ntp.org), scarica il file da GitHub e controlla lunghezza e SHA-256 prima di avviarlo."))+'</p>'
  +'<p class="law" style="margin:0 0 8px">'+esc(t("Il tuo indirizzo IP pubblico sara visibile a GitHub e al server dell'ora. Succede solo adesso, perche lo hai chiesto: la scheda non controlla gli aggiornamenti da sola."))+'</p>'
  +'<label for="unu">'+esc(t("Indirizzo del file (https)"))+'</label><input id="unu" autocomplete="off" spellcheck="false" maxlength="199" placeholder="'+esc(j.prefix||"")+'..." value="'+esc(j.prefix||"")+'">'
  +'<label for="uns">'+esc(t("Impronta SHA-256 del file (64 cifre)"))+'</label><input id="uns" autocomplete="off" spellcheck="false" maxlength="64">'
  +'<label for="unv">'+esc(t("Versione (facoltativa)"))+'</label><input id="unv" autocomplete="off" spellcheck="false" maxlength="23">'
  +'<p style="margin:8px 0">'+esc(t("Copia indirizzo e impronta dalla pagina della versione su GitHub. Se qualcosa va storto la scheda resta nel recovery: da li puoi caricare il file a mano."))+'</p>'
  +'<label class="chk" for="updc"><input type="checkbox" id="updc"><span>'+esc(t("Ho capito: la scheda sara fuori servizio per alcuni minuti"))+'</span></label>',ok:t("Riavvia e aggiorna"),
  onOk:function(){var u=dlgV("unu"),h=dlgV("uns"),v=dlgV("unv");
   if(u.indexOf("https://")!=0)return t("L'indirizzo deve cominciare con https://");
   if(!/^[0-9a-fA-F]{64}$/.test(h))return t("L'impronta SHA-256 deve avere 64 cifre esadecimali");
   if(v&&!/^[0-9A-Za-z._-]{1,23}$/.test(v))return t("La versione non e valida");
   if(!$("updc").checked)return t("Spunta la casella per continuare");
   return post("/api/updorder/net",{confirm:"1",url:u,sha:h,ver:v}).then(function(r){return r.json()}).then(function(k){
    if(k.ok)setTimeout(updWaitBack,50);return k.ok?"":(k.err||t("Errore"))},function(){return t("Richiesta non riuscita")})}})}
/* aspetta che la scheda sparisca (recovery) e torni (VesevOS): /api/auth ha il campo "pk" solo in VesevOS */
function updWaitBack(){var t0=Date.now(),down=false;
 dlg({title:t("Aggiornamento in corso"),body:'<p style="margin:0" role="status">'+esc(t("La scheda e nel recovery: scarica e controlla il file. Puo durare alcuni minuti. Non spegnerla. Questa pagina si ricarica da sola quando VesevOS e di nuovo pronto."))+'</p>',buttons:[]});
 (function tick(){
  if(Date.now()-t0>1200000){dlg({title:t("Aggiornamento in corso"),body:'<p style="margin:0">'+esc(t("La scheda non e tornata dopo 20 minuti. Apri l'indirizzo della scheda con http (non https): se e nel recovery, la pagina mostra lo stato e puoi caricare il file a mano."))+'</p>',buttons:[["ok",t("Chiudi"),""]]});return}
  var c=new AbortController(),m=setTimeout(function(){c.abort()},4000);
  fetch("/api/auth",{cache:"no-store",signal:c.signal}).then(function(r){return r.json()}).then(function(a){clearTimeout(m);
   if(a&&a.pk){if(down)location.reload();else setTimeout(tick,2000)}else{down=true;setTimeout(tick,2000)}},function(){clearTimeout(m);down=true;setTimeout(tick,2000)})})()}
