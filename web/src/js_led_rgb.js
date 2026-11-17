/* ===== periferica LED a colori a mano (classe "led_rgb", WS2812): voce nell'elenco di aggiunta e riquadro di prova; il firmware la offre solo se la include e ci sono canali liberi ===== */
function ioRgbOpt(free){
  var c=$("io_c"),o=c.querySelector('option[value="led_rgb"]');
  if(free===undefined)return;                       // firmware senza LED a colori a mano: nessuna voce
  if(!o){
    o=new Option("","led_rgb");
    c.add(o)
  }
  o.disabled=free<1;
  o.textContent=free<1?t("LED a colori (canali finiti)"):t("LED a colori (WS2812)");
  if(o.disabled&&c.value=="led_rgb"){
    c.value="rele";
    ioClsChg()
  }
}

function ioRgbHex(d){return("000000"+d.col.toString(16)).slice(-6)}

function ioRgbOpts(){
  var o=$("io_ro").value;
  return o?"ordine="+o:""
}

function ioRgbDet(d){
  var h='<div class="ftb"><button class="btn" onclick="ioAct(\'set\',1)">'+esc(t("Accendi"))+
    '</button><button class="btn gray" onclick="ioAct(\'set\',0)">'+esc(t("Spegni"))+'</button></div>';
  if(ROLE>=1)h+='<label for="rg_c">'+esc(t("Colore"))+'</label><input type="color" id="rg_c" value="#'+
    ioRgbHex(d)+'" onchange="ioRgbSet()"><label for="rg_l">'+esc(t("Luminosita"))+
    ' <b id="rg_lv">'+d.lum+'</b></label><input type="range" id="rg_l" min="0" max="255" value="'+d.lum+
    '" oninput="$(\'rg_lv\').textContent=this.value" onchange="ioRgbSet()">';
  if(ROLE>=2)h+='<label for="rg_o">'+esc(t("Ordine dei byte"))+'</label><select id="rg_o" onchange="ioAct(\'ordine\',this.value)">'+
    ["grb","rgb","brg","gbr","rbg","bgr"].map(function(x){
      return'<option value="'+x+'"'+(x==d.ord?" selected":"")+'>'+x.toUpperCase()+'</option>'
    }).join("")+'</select>';
  return h+'<p class="fhint">'+esc(t(
    "Il LED torna com'era anche dopo il riavvio. Se i colori sono scambiati (per esempio il rosso diventa verde) cambia l'ordine dei byte."))+'</p>'
}

function ioRgbSet(){
  ioAct("colore",$("rg_c").value.slice(1)+","+$("rg_l").value)
}
