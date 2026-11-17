/* ===== periferica PWM (classe "pwm"): voce nell'elenco di aggiunta e riquadro di prova; il firmware la offre solo se la include e ci sono canali liberi ===== */
function ioPwmVal(d){return d.v+"% · "+d.hz+" Hz"}

function ioPwmOpt(free){
  var c=$("io_c"),o=c.querySelector('option[value="pwm"]');
  if(free===undefined)return;                       // firmware senza PWM: nessuna voce
  if(!o){
    o=new Option("PWM","pwm");
    c.add(o)
  }
  o.disabled=free<1;
  o.textContent=free<1?t("PWM (canali finiti)"):"PWM";
  if(o.disabled&&c.value=="pwm"){
    c.value="rele";
    ioClsChg()
  }
}

function ioPwmDet(d){
  var h='<label for="pw_d">'+esc(t("Duty"))+' <b id="pw_dv">'+d.v+'%</b></label>'+
    '<input type="range" id="pw_d" min="0" max="100" value="'+d.v+
    '" oninput="$(\'pw_dv\').textContent=this.value+\'%\'" onchange="ioPwmSet()">';
  if(ROLE>=2)h+='<label for="pw_h">'+esc(t("Frequenza (Hz)"))+
    '</label><input type="number" id="pw_h" min="10" max="40000" value="'+d.hz+
    '" style="width:7em"> <button class="btn gray" onclick="ioPwmSet()">'+esc(t("Applica"))+'</button>';
  return h+'<p class="fhint">'+esc(tf(
    "Dopo il riavvio il PWM riparte spento. Nelle app: use={0} e poi pwm(\"{0}\", 50).",d.n))+'</p>'
}

function ioPwmSet(){
  var h=$("pw_h");
  ioAct("set",$("pw_d").value+(h?","+h.value:""))
}
