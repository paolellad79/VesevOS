/* funzioni scritte sulla foto della scheda: l'ESP32-S3 puo quasi tutto su ogni pin, quindi sono indicative */
var PFN={1:"UART PWM I2S ADC I2C SPI",2:"UART PWM I2S ADC I2C SPI",3:"UART PWM I2S ADC I2C SPI",
  4:"UART PWM I2S ADC I2C SPI",5:"UART PWM I2S ADC I2C SPI",6:"UART PWM I2S ADC I2C SPI",
  7:"UART PWM I2S ADC I2C SPI",8:"SPI I2C ADC I2S PWM UART",9:"SPI I2C ADC I2S PWM UART",
  10:"SPI I2C ADC I2S PWM UART",11:"SPI I2C ADC I2S PWM UART",12:"SPI I2C ADC I2S PWM UART",
  13:"SPI I2C ADC I2S PWM UART",14:"UART PWM I2S ADC I2C SPI",15:"UART PWM I2S ADC I2C SPI",
  16:"UART PWM I2S ADC I2C SPI",17:"GPIO",18:"GPIO",21:"GPIO",33:"UART PWM I2S I2C SPI",
  34:"UART PWM I2S I2C SPI",35:"SPI I2C I2S PWM UART",36:"SPI I2C I2S PWM UART",
  37:"UART PWM I2S I2C SPI",38:"UART PWM I2S I2C SPI",39:"SPI I2C I2S PWM UART",
  40:"SPI I2C I2S PWM UART",41:"SPI I2C I2S PWM UART",42:"SPI I2C I2S PWM UART",43:"UART0 TX",
  44:"UART0 RX",45:"SPI I2C I2S PWM UART",46:"SPI I2C I2S PWM UART",47:"SPI I2C I2S PWM UART",
  48:"SPI I2C I2S PWM UART",0:"BOOT"};
function pinWarnTxt(g){
  var w=[];
  if(!PBI)return"";
  if((PBI.strap||[]).indexOf(g)>=0)w.push(t("Pin di avvio (strapping): meglio non usarlo"));
  if(brdIn("adc2",g))w.push(t("ADC2: con il Wi-Fi acceso la lettura analogica non e affidabile"));
  if(brdIn("jtag",g))w.push(t("Usato dal debug JTAG se lo si usa"));
  if((PBI.usb||[]).indexOf(g)>=0)w.push(t("USB: non usabile"));
  if((PBI.uart||[]).indexOf(g)>=0)w.push(t("Console seriale: evitare"));
  if(brdIn("psq",g))w.push(
    t("Libero con la PSRAM da 2 MB di questa scheda; occupato sulle schede con PSRAM octal"));
  return w.join(". ")
}

function pinPos(g){
  var s=String(g);
  return brdLay("fl").indexOf(s)>=0||brdLay("fr").indexOf(s)>=0?t("bordo")
    :brdLay("bl").indexOf(s)>=0||brdLay("br").indexOf(s)>=0?t("retro (piazzola)"):""
}

function loadPins(){
  brdLoad().then(function(b){
    if(!b)return;
    $("pinchip").textContent=
      tf(
        "Rilevato: {0} rev {1}, {2} core, {3} GPIO nel chip ({4} usabili come uscita). Scheda: {5}.",
        b.chip,b.rev,b.cores,b.gpioCount,b.gpioOut,b.board);
    drawPins()
  }).catch(function(){});
  api("/api/pins").then(function(r){return r.json()}).then(function(p){
    PUSE=p;
    var h="<tr><th>GPIO</th><th>"+t("Usato da")+"</th><th>"+t("Descrizione")+"</th></tr>";
    p.forEach(function(x){
      h+="<tr><td>"+x.gpio+"</td><td>"+esc(x.owner)+"</td><td>"+esc(x.note)+"</td></tr>"
    });
    $("pintab").innerHTML=h;
    if(PM.length)drawPins()
  });
  api("/api/pinmap").then(function(r){return r.json()}).then(function(m){
    PM=m;
    drawPins();
    pinPanel();
    pinInv()
  });
  api("/api/pinnotes").then(function(r){return r.json()}).then(function(n){
    PNOTE=n;
    drawPins();
    pinInv()
  });
  pinViewBtns();
  pinDevLoad();
  clearInterval(PTM);
  PTM=setInterval(pinPoll,1000);
  pinPoll()
}

function pinViewBtns(){
  $("pinview").innerHTML='<button class="'+(PV=="f"?"on":"")+'" onclick="pinV(\'f\')">'+
    esc(t("Fronte"))+'</button><button class="'+(PV=="b"?"on":"")+'" onclick="pinV(\'b\')">'+
    esc(t("Retro"))+'</button>'
}

function pinV(v){
  PV=v;
  pinViewBtns();
  drawPins()
}

function pinInfo(g){
  for(var i=0;i<PM.length;i++)if(PM[i].g==g)return PM[i];
  return null
}
var PUSE=[];
function pinOwner(g){
  for(var i=0;i<PUSE.length;i++)if(PUSE[i].gpio==g)return PUSE[i];
  return null
}

/* Il disegno si prepara solo a scheda aperta: chiusa non si tocca e nulla viene misurato. */
function pinDisegnoAperto(){
  return cur=="pin"&&!$("sp_pin0").classList.contains("hide")
}

/* Si torna sulla scheda Pin: disegno e pannello riprendono dati e tema di adesso. */
function pinSchedaDisegno(){
  drawPins();
  pinPanel()
}

function drawPins(){
  if(!pinDisegnoAperto())return;
  var e=$("pinsvg");
  if(!PBI||!PBI.lay){
    e.innerHTML="";
    return
  }
  var FL=brdLay("fl"),FR=brdLay("fr"),BL=brdLay("bl").map(Number),BR=brdLay("br").map(Number),
    LED=PBI.led,UA=PBI.uart||[];
  var W=400,bw=190,top=64,st=26,bh=pinBoardH(bw,Math.max(FL.length,FR.length,1)),s="",ink=pinInk();
  var fr=PV=="f",L=fr?FL:FR,R=fr?FR:FL,plan=fr?pinSchemaPlan(L,R,top,st,bw):null,
    bx=plan?plan.bx:105;
  s+=pinBoardArt(fr,bx,top,bw,bh,ink,LED);
  function col(g,x2,u){
    var nt=PNOTE[g];
    return g==PT.gpio?"#e0a020":x2.ok?(nt?"#9b6bd6":"#2e9e5b"):(x2.owner||u)?"#3fa7ff":nt?"#9b6bd6"
      :"#3a4352"
  }
  function tip(g,x2,u){
    return"GPIO"+g+" - "+(PFN[g]||"")+(x2.why?" - "+x2.why:"")+
      (u?" - "+u.owner+(u.sig?" ("+u.sig+")":""):"")+(PNOTE[g]?" - "+PNOTE[g]:"")+
      (pinWarnTxt(g)?" - "+pinWarnTxt(g):"")
  }
  function one(lab,x,y,side){
    var g=+lab,pw=PWR[lab];
    if(pw){
      s+='<circle cx="'+x+'" cy="'+y+'" r="11" fill="'+pw+'" stroke="#555"/><text x="'+x+'" y="'+
        (y+3)+'" font-size="7.5" text-anchor="middle" fill="#fff">'+lab+'</text>';
      return
    }
    var x2=pinInfo(g)||{g:g,ok:false},u=pinOwner(g),c=col(g,x2,u),nm=g==UA[0]?"TX":g==UA[1]?"RX":"";
    s+='<g style="cursor:pointer" onclick="pinSel('+g+
      ')" role="button" tabindex="0" onkeydown="pinKey(event,'+g+')" aria-label="'+esc(tip(g,x2,u))+
      '"><circle cx="'+x+'" cy="'+y+'" r="11" fill="'+c+'" stroke="'+(g==PSEL?"#fff":"#0b0f14")+
      '" stroke-width="'+(g==PSEL?2.5:1)+'"/><text x="'+x+'" y="'+(y+3)+
      '" font-size="8.5" font-weight="700" text-anchor="middle" fill="#fff">'+g+'</text><title>'+
      esc(tip(g,x2,u))+'</title></g>';
    if(nm&&fr)s+='<text x="'+(x+(side<0?16:-16))+'" y="'+(y+3)+'" font-size="7" text-anchor="'+
      (side<0?"start":"end")+'" fill="'+ink+'">'+nm+'</text>';
    if(plan&&plan.pinBlk[g]){
      var sg=plan.pinSig[g];
      if(sg)s+='<text x="'+(x+side*14)+'" y="'+(y-3)+'" font-size="7" text-anchor="'+
        (side<0?"end":"start")+'" fill="'+c+'">'+esc(sg)+'</text>'
    }
  }
  L.forEach(function(l,i){one(l,bx,top+PY0+i*st,-1)});
  R.forEach(function(l,i){one(l,bx+bw,top+PY0+i*st,1)});
  if(!fr){
    [[BL,-1],[BR,1]].forEach(function(cl){
      cl[0].forEach(function(g,i){
        var xy=pinPadXY(g,i,cl[1],bx,top,bw,bh),x2=pinInfo(g)||{g:g,ok:false},u=pinOwner(g),
          c=col(g,x2,u);
        s+='<g style="cursor:pointer" onclick="pinSel('+g+
          ')" role="button" tabindex="0" onkeydown="pinKey(event,'+g+')" aria-label="'+
          esc(tip(g,x2,u))+'"><rect x="'+(xy[0]-12)+'" y="'+(xy[1]-8)+
          '" width="24" height="16" rx="4" fill="'+c+'" stroke="'+(g==PSEL?"#fff":"#0b0f14")+
          '" stroke-width="'+(g==PSEL?2.5:1)+'"/><text x="'+xy[0]+'" y="'+(xy[1]+3)+
          '" font-size="8" font-weight="700" text-anchor="middle" fill="#fff">'+g+'</text><title>'+
          esc(tip(g,x2,u))+'</title></g>'
      })
    })
  }
  if(plan)s+=pinSchemaSvg(plan);
  var x0=plan?plan.xMin:0,Wd=plan?plan.w:W,ly=Math.max(top+bh,plan?plan.bottom:0)+26;
  [["#2e9e5b",t("provabile")],["#3fa7ff",t("in uso")],["#9b6bd6",t("collegato (tuo nome)")],
    ["#3a4352",t("vietato")],["#e0a020",t("in prova")]].reduce(function(lx,l){
      s+='<circle cx="'+lx+'" cy="'+ly+'" r="5" fill="'+l[0]+'"/><text x="'+(lx+9)+'" y="'+(ly+3)+
        '" font-size="8" fill="'+ink+'">'+esc(l[1])+'</text>';
      return lx+23+l[1].length*4.4
    },x0+14);
  if(!fr){
    var bt=t(
      "Ponticello BOOST: la corrente di carica passa da 100 a 300 mA. Solo con una batteria oltre 500 mAh."),
      bk=bt.indexOf(". ")+1,bw2=bk>0?[bt.slice(0,bk),bt.slice(bk)]:[bt];
    bw2.forEach(function(r,i){
      s+='<text x="'+(x0+Wd/2)+'" y="'+(ly+20+i*11)+
        '" font-size="8" text-anchor="middle" fill="#d9a521">'+esc(r.trim())+'</text>'
    })
  }
  if(plan)s+='<text x="'+(x0+Wd/2)+'" y="'+(ly+34)+'" font-size="8" text-anchor="middle" fill="'+
    ink+'">'+esc(t("Filo tratteggiato: dato mancante, da verificare."))+'</text>';
  e.style.maxWidth=plan?"840px":"520px";
  e.style.minWidth=plan?"640px":"";
  e.setAttribute("viewBox",x0+" 0 "+Wd+" "+(ly+(plan?44:fr?30:42)));
  e.innerHTML=s
}

function pinInv(){
  var h="<tr><th>GPIO</th><th>"+esc(t("Posizione"))+"</th><th>"+esc(t("Funzioni"))+"</th><th>"+
    esc(t("Stato"))+"</th><th>"+esc(t("Collegato a"))+"</th></tr>",
    gs=PM.map(function(x){return x.g}).sort(function(a,b){return a-b});
  gs.forEach(function(g){
    var x=pinInfo(g),u=pinOwner(g),st=u?u.owner:(x.ok?t("libero"):(x.why||t("vietato"))),
      w=pinWarnTxt(g);
    h+="<tr><td><b>"+g+"</b></td><td>"+esc(pinPos(g))+"</td><td>"+esc(PFN[g]||"")+
      (w?'<div class="fhint">'+esc(w)+'</div>':"")+"</td><td>"+esc(st)+"</td><td>"+
      (ROLE>=2?'<input maxlength="24" value="'+esc(PNOTE[g]||"")+'" aria-label="GPIO '+g+
        '" onchange="pinNote('+g+',this)" style="min-width:120px">':esc(PNOTE[g]||""))+"</td></tr>"
  });
  $("pininv").innerHTML=h
}

function pinNote(g,e){
  post("/api/pinnotes",{g:g,name:e.value}).then(function(r){return r.json()}).then(function(j){
    if(j.ok===false){
      msg("pinvm",j.err,false);
      return
    }
    PNOTE[g]=e.value.trim();
    if(!PNOTE[g])delete PNOTE[g];
    msg("pinvm",t("Salvato"),true);
    drawPins()
  }).catch(function(){})
}

function pinSvgDl(){
  var x='<?xml version="1.0" encoding="UTF-8"?>\n<svg xmlns="http://www.w3.org/2000/svg" viewBox="'+
    $("pinsvg").getAttribute("viewBox")+
    '" font-family="Arial,sans-serif"><rect x="-1000" y="-1000" width="4000" height="4000" fill="'+
    (pinInk()=="#566676"?"#ffffff":"#10151c")+'"/>'+$("pinsvg").innerHTML+'</svg>';
  var a=document.createElement("a");
  a.href=URL.createObjectURL(new Blob([x],{type:"image/svg+xml"}));
  a.download="vesevos-pin.svg";
  document.body.appendChild(a);
  a.click();
  setTimeout(function(){
    URL.revokeObjectURL(a.href);
    a.remove()
  },500)
}

function pinSel(g){
  PSEL=g;
  drawPins();
  pinPanel()
}

function pinPanel(){
  var e=$("pinpanel"),h="";
  h+='<label for="pn_g">'+esc(t("Altro GPIO"))+
    '</label><input type="number" id="pn_g" min="0" max="'+brdMax()+'" value="'+(PSEL<0?"":PSEL)+
    '" style="width:90px" onchange="pinSel(parseInt(this.value))"> ';
  var x=pinInfo(PSEL);
  if(PSEL<0){
    e.innerHTML=h;
    return
  }
  h+='<h3 style="margin-top:10px">GPIO'+PSEL+'</h3>'+pinWho(PSEL);
  if(PFN[PSEL])h+='<div class="fhint">'+esc(PFN[PSEL])+
    (pinWarnTxt(PSEL)?" - "+esc(pinWarnTxt(PSEL)):"")+'</div>';
  if(PNOTE[PSEL])h+='<div class="law">'+
    esc(tf("Segnato come collegato a: {0}. Una prova puo disturbare il circuito.",PNOTE[PSEL]))+
    '</div>';
  if(!x){
    h+='<div class="msg ko">'+esc(t("Pin inesistente su questa scheda"))+'</div>'
  }
  else if(!x.ok&&PSEL!=PT.gpio){
    h+='<div class="msg ko">'+esc(t("Non provabile:"))+" "+esc(x.why)+'</div>'
  }
  else{
    h+='<div id="pnlv" style="margin:6px 0"></div>';
    h+='<button class="btn" onclick="pinGo(\'high\')">'+esc(t("Alto (3,3 V)"))+
      '</button><button class="btn gray" onclick="pinGo(\'low\')">'+esc(t("Basso (0 V)"))+
      '</button><button class="btn" onclick="pinGo(\'blink\')">'+esc(t("Lampeggia"))+'</button>';
    h+='<div style="margin-top:8px"><button class="btn" onclick="pinGo(\'read\')">'+esc(t("Leggi"))+
      '</button> <select id="pn_pull"><option value="none">'+esc(t("senza resistenza"))+
      '</option><option value="up">'+esc(t("resistenza verso 3,3 V"))+
      '</option><option value="down">'+esc(t("resistenza verso massa"))+'</option></select></div>';
    h+='<div style="margin-top:8px"><button class="btn red" onclick="pinGo(\'off\')">'+
      esc(t("Rilascia"))+'</button></div>';
    h+='<div id="pnm" class="msg"></div>'
  }
  e.innerHTML=h;
  pinLevel()
}

function pinWarn(){
  if(PWARN)return true;
  var w=[t("ATTENZIONE: una prova sbagliata puo danneggiare la scheda."),"",
    t("- Usa solo 3,3 V. Mai 5 V o tensioni piu alte su un pin."),
    t("- Ogni pin regge pochi mA: un LED vuole una resistenza da 220-470 ohm."),
    t("- Motori, relè e carichi grandi vanno pilotati con un transistor."),
    t("- Non collegare due uscite insieme e non toccare i pin di alimentazione."),"",
    t("Vuoi continuare?")].join("\n");
  if(!confirm(w))return false;
  PWARN=true;
  return true
}

function pinGo(a){
  if(PSEL<0)return;
  if(a!="off"&&!pinWarn())return;
  var d={gpio:PSEL,action:a};
  if(a=="read")d.pull=$("pn_pull").value;
  post("/api/pintest",d).then(function(r){return r.json()}).then(function(j){
    if(!j.ok){
      msg("pnm",j.err,false);
      return
    }
    setTimeout(pinPoll,300)
  })
}

function pinPoll(){
  if(cur!="pin"){
    clearInterval(PTM);
    return
  }
  api("/api/pintest").then(function(r){return r.json()}).then(function(j){
    var ch=(j.gpio!=PT.gpio);
    PT=j;
    if(ch){
      api("/api/pinmap").then(function(r){return r.json()}).then(function(m){
        PM=m;
        drawPins();
        pinPanel()
      })
    }else pinLevel()
  }).catch(function(){})
}

function pinLevel(){
  var e=$("pnlv");
  if(!e)return;
  if(PT.gpio!=PSEL){
    e.textContent=t("Nessuna prova in corso su questo pin");
    return
  }
  var an={high:t("Alto"),low:t("Basso"),blink:t("Lampeggia"),read:t("Lettura")}[PT.action];
  e.innerHTML=esc(an)+" - "+esc(t("livello letto"))+": <b>"+(PT.level?t("ALTO"):t("BASSO"))+
    "</b> - "+esc(tf("si spegne tra {0} s",PT.left))
}
