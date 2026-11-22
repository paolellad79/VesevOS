/* ===== Coprocessore (sola lettura) ===== */
function coLoad(){
  api("/api/coproc").then(function(r){return r.json()}).then(coDraw).catch(function(){})
}

function coDraw(j){
  var tipo=j.ulp?t("ULP (RISC-V o FSM)"):j.lp?t("LP CPU (RISC-V)"):t("Nessuno");
  $("co_tipo").textContent=tipo;
  $("co_mem").textContent=j.mem>0?tf("{0} byte",j.mem):t("non dichiarata");
  $("co_prog").textContent=j.prog?t("Programma caricato"):t("Nessun programma caricato");
  var w=[t("Accensione o riavvio (non dal sonno)"),t("Timer"),t("Piedino (GPIO)"),
    t("Sensore di tocco"),t("Coprocessore"),t("Porta seriale")];
  $("co_wake").textContent=j.wake>=0&&j.wake<=5?w[j.wake]:t("Altro")
}
