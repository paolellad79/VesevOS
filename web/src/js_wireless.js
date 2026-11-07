/* Interruttore radio generale (Dispositivi > Interfacce > Radio).
   Usa il componente dei dispositivi (dvSw) e il modo aereo gia esistente (/api/airplane). */
function radLoad(){
 var on=!(RN.n&&RN.n.air);
 $("rad_sw").innerHTML=dvSw(on,t("Radio generale"),"radSave("+(on?0:1)+")");
}

/* on = nuovo stato: acceso -> esce dal modo aereo; spento -> entra (fisso: solo a mano) */
function radSave(on){
 if(on) return radSend(0);
 dlg({title:t("Spegnere la radio?"),
  body:'<p style="margin:0">'+esc(t("Con la radio spenta la pagina non si raggiunge. Per riaccenderla: tasto fisico, seriale (airplane off) o regola nelle Automazioni."))+'</p>',
  ok:t("Spegni"), danger:true,
  onOk:function(){ radSend(1); return "" }})
}

function radSend(air){
 post("/api/airplane",{on:air,exit:3,param:0}).then(function(r){return r.json()}).then(function(j){
  if(!j.ok){ msg("radm",j.err,false); return }
  if(air) return;
  msg("radm",t("Salvato"),true);
  setTimeout(radLoad,600)
 }).catch(function(){ msg("radm",t("Errore"),false) })
}
