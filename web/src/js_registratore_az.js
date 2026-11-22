/* ===== Registratore di dati: azioni (eventi.csv) ===== */
/* un bit per categoria: lo stesso ordine di AzioneCat in core/vos_azioni.h */
var RCAZ=["uscita","pwm","led","contatore","programma","riavvio","sonno","avvio","rete","servizio","sd"];

function rcAzNome(i){
  var n=[t("Uscite e rele"),t("PWM"),t("LED"),t("Contatore azzerato"),t("Script, app e programmi"),t("Riavvii"),t("Sonno profondo"),
    t("Avvio e risvegli"),t("Rete (Wi-Fi e hotspot)"),t("Servizi"),t("microSD")];
  return n[i]
}

function rcAzOrigine(o){
  var m={sistema:t("sistema"),shell:t("shell"),pagina:t("pagina"),script:t("script"),regola:t("regola")};
  return m[o]||o
}

function rcAzValore(cat,v,txt){
  if(cat=="avvio"&&txt)return txt;
  if(cat=="uscita"||cat=="led"||cat=="servizio")return v?t("acceso"):t("spento");
  if(cat=="programma")return v==1?t("avviato"):v==2?t("finito con errore"):t("finito");
  if(cat=="sd")return v==1?t("montata"):v==2?t("formattata"):t("smontata");
  return ""+v
}

function rcAzDraw(j){
  var e=j.ev;
  if(!e)return;
  $("rc_azst").textContent=tf("{0} azioni registrate dall'avvio, {1} perse per coda piena, {2} oltre il limite del giorno",e.tot,e.persi,e.scartati)+
    (e.err?" · "+e.err:"");
  var h="<tr><th scope=\"col\">"+esc(t("Quando"))+"</th><th scope=\"col\">"+esc(t("Categoria"))+"</th><th scope=\"col\">"+esc(t("Nome"))+
    "</th><th scope=\"col\">"+esc(t("Valore"))+"</th><th scope=\"col\">"+esc(t("Origine"))+"</th></tr>",i,r;
  for(i=0;i<e.ultime.length;i++){
    r=e.ultime[i];
    h+="<tr><td>"+esc(rcTime(r))+"</td><td>"+esc(rcAzNome(RCAZ.indexOf(r[2])))+"</td><td>"+esc(r[3])+"</td><td>"+
      esc(rcAzValore(r[2],r[4],r[6]))+"</td><td>"+esc(rcAzOrigine(r[5]))+"</td></tr>"
  }
  $("rc_aztab").innerHTML=e.ultime.length?h:"";
  if(!e.ultime.length)$("rc_azst").textContent+=" · "+t("Nessuna azione ancora");
  var f=j.files,l="";
  for(i=0;i<f.length;i++)if(f[i].e)
    l+='<a href="/api/fs/get?path='+enc(j.dir+"/"+f[i].n)+'" download="'+esc(f[i].n)+'">'+esc(f[i].n)+"</a> ";
  $("rc_azfiles").innerHTML=l?esc(t("Scarica CSV degli eventi"))+": "+l:""
}

/* caselle delle categorie e tetto giornaliero: si riempiono solo se non si sta scrivendo nel modulo */
function rcAzForm(j){
  var h="",i;
  for(i=0;i<RCAZ.length;i++)
    h+='<label><input type="checkbox" id="rc_a'+i+'"'+((j.azioni>>i)&1?" checked":"")+"> "+esc(rcAzNome(i))+"</label>";
  $("rc_acats").innerHTML=h;
  $("rc_evmax").value=j.evmax
}

function rcAzMask(){
  var m=0,i;
  for(i=0;i<RCAZ.length;i++)if($("rc_a"+i).checked)m|=1<<i;
  return m
}
