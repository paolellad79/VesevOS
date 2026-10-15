/* ===== Tasto BOOT: azioni dei click ===== */
var BT={j:null,tm:null,seq:-1};
function btnLoad(){clearTimeout(BT.tm);api("/api/btn").then(function(r){return r.json()}).then(btnDraw).catch(function(){if(cur=="btn")BT.tm=setTimeout(btnLoad,2000)})}
function btnDraw(j){BT.j=j;
 for(var n=1;n<=3;n++){var s=$("bt_c"+n);if(!s)continue;var h="";j.opt.forEach(function(o){h+='<option value="'+o.id+'">'+esc(o.label)+'</option>'});s.innerHTML=h;s.value=String(j.act[n-1])}
 var w=$("bt_w"),usa=j.wpsClicks>0&&!j.wpsOn;w.className=usa?"fhint law":"fhint law hide";
 w.textContent=usa?t("Il WPS e spento: i click non lo avviano finche non lo accendi in Rete > WPS."):"";
 var fr=[];j.free.forEach(function(f,i){if(f)fr.push(i+1)});
 $("bt_f").textContent=fr.length?tf("Liberi per gli script: {0} click (funzione btn()). Chi li legge e uno script in esecuzione.",fr.join(", ")):t("Nessun gesto libero per gli script: metti \"Libero\" su un gesto per dargli i click.");
 var on=j.test>0,s=$("bt_ts");$("bt_t").textContent=on?t("Ferma la prova"):t("Avvia la prova");
 s.className=on?"msg ok":"msg";s.style.display=on?"block":"none";
 s.textContent=on?tf("Prova in corso: ancora {0} s. Premi il tasto.",j.test)+" "+(j.down?tf("Premuto da {0} s",(j.heldMs/1000).toFixed(1)):t("Rilasciato")):"";
 $("bt_tl").textContent=on?j.last||"":"";
 clearTimeout(BT.tm);if(cur=="btn"&&on)BT.tm=setTimeout(btnLoad,300)}
function btnTest(){var on=!(BT.j&&BT.j.test>0);post("/api/btn",{test:on?1:0}).then(function(r){return r.json()}).then(btnDraw).catch(function(){})}
function btnSave(n,e){var d={};d["c"+n]=e.value;post("/api/btn",d).then(function(r){return r.json()}).then(function(j){if(j.ok===false){msg("btm",j.err,false);btnLoad();return}btnDraw(j);msg("btm",t("Salvato"),true)}).catch(function(){})}
