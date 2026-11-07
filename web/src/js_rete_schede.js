/* ===== Rete tra schede ===== */
var MS={d:null,tm:null};
var MROLE=["Nodo","Gateway","Sensore"];
function msLoad(){
  clearInterval(MS.tm);
  msPoll();
  MS.tm=setInterval(function(){
    if(cur!="mesh"){
      clearInterval(MS.tm);
      return
    }
    msPoll()
  },4000);
}

function msPoll(){
  api("/api/mesh").then(function(r){return r.json()}).then(function(m){
    MS.d=m;
    svcSt("mesh",!!m.run);
    $("msst").innerHTML='<span class="fhint">'+
      esc(tf("Canale {0} · inviati {1}, ricevuti {2}, scartati {3}",m.ch,m.sent,m.recv,m.bad))+
      '</span>'+(m.hasKey?"":'<div class="msg ko" style="display:block;margin-top:6px">'+
        esc(t("Manca la chiave comune: creala in Impostazioni"))+'</div>');
    var foc=document.activeElement&&document.activeElement.closest&&
      document.activeElement.closest("#t_mesh .card.adm");
    if(!foc&&ROLE==2){
      $("ms_role").value=m.role;
      $("ms_auto").classList.toggle("on",m.auto);
      var h="";
      for(var c=1;c<=m.chMax;c++)h+='<option value="'+c+'"'+(c==m.ch?" selected":"")+'>'+c+
        '</option>';
      $("ms_ch").innerHTML=h
    }
    var tb='<thead><tr><th>'+esc(t("Nome"))+'</th><th>'+esc(t("Ruolo"))+'</th><th>'+esc(t("Salti"))+
      '</th><th>'+esc(t("Segnale"))+'</th><th>'+esc(t("Visto"))+'</th><th>'+esc(t("Stato"))+
      '</th></tr></thead><tbody>';
    if(!m.nodes.length)tb+='<tr><td colspan="6" style="color:var(--mut)">'+
      esc(t("Nessuna scheda vicina"))+'</td></tr>';
    m.nodes.forEach(function(n){
      tb+='<tr><td><b>'+esc(n.name||"-")+'</b><br><code class="mac">'+esc(n.mac)+'</code></td><td>'+
        esc(t(MROLE[n.role]||"?"))+'</td><td>'+n.hops+'</td><td>'+n.rssi+' dBm, '+
        esc(msQual(n.rssi)[0])+'</td><td>'+
        esc(durText(n.ago))+'</td><td>'+esc(n.state)+
        (n.ccx?' <span class="pill k">'+esc(t("altro paese"))+'</span>':"")+'</td></tr>'
    });
    $("mstab").innerHTML=tb+'</tbody>';
    $("msmap").innerHTML=msMappa(m.nodes);
    var to=$("ms_to"),sv=to.value,o='<option value="*">'+esc(t("Tutte"))+'</option>';
    m.nodes.forEach(function(n){
      o+='<option value="'+esc(n.mac)+'">'+esc(n.name||n.mac)+'</option>'
    });
    to.innerHTML=o;
    if(sv)to.value=sv;
    if(!to.value)to.value="*";
    var mh="";
    m.msgs.forEach(function(x){
      mh+='<div class="row"><span><b>'+esc(x.from)+'</b> '+esc(x.text)+
        '</span><span class="fhint">'+esc(x.when)+'</span></div>'
    });
    $("msmsg").innerHTML=mh
  }).catch(function(){})
}

/* Qualita del segnale: parola e colore. */
function msQual(r){
  if(r>=-65)return[t("buono"),"var(--ok)"];
  if(r>=-80)return[t("medio"),"var(--wa)"];
  return[t("debole"),"var(--ko)"]
}
/* Topologia: questa scheda al centro, le altre attorno. La tabella sotto dice le stesse cose. */
function msMappa(L){
  if(!L.length)return"";
  var cx=180,cy=120,h="",dida=[],n=L.length;
  L.forEach(function(x,i){
    var a=2*Math.PI*i/n-Math.PI/2,px=Math.round(cx+130*Math.cos(a)),py=Math.round(cy+84*Math.sin(a)),
      q=msQual(x.rssi),nome=x.name||x.mac;
    dida.push(tf("{0}: segnale {1}, {2} dBm",nome,q[0],x.rssi));
    h+='<line x1="'+cx+'" y1="'+cy+'" x2="'+px+'" y2="'+py+'" stroke="'+q[1]+
      '" stroke-width="2"'+(x.hops>1?' stroke-dasharray="5 4"':"")+'/>'+
      '<text x="'+Math.round((cx+px)/2)+'" y="'+(Math.round((cy+py)/2)-3)+
      '" text-anchor="middle">'+x.rssi+' dBm</text>'+
      '<circle cx="'+px+'" cy="'+py+'" r="6" fill="'+q[1]+'"/>'+
      '<text x="'+px+'" y="'+(py+(py>cy?18:-10))+'" text-anchor="middle">'+
      esc(nome.slice(0,14)+" · "+q[0])+'</text>'
  });
  return'<svg class="msm" viewBox="0 0 360 240" role="img" aria-label="'+esc(dida.join(". "))+'">'+
    h+'<circle cx="'+cx+'" cy="'+cy+'" r="9" fill="var(--ac)"/><text x="'+cx+'" y="'+(cy+22)+
    '" text-anchor="middle">'+esc(t("Questa scheda"))+'</text></svg>'
}

function msRun(a){
  return post("/api/mesh/run",{a:a}).then(function(r){return r.json()}).then(function(j){
    msg("msm",j.ok?t("Fatto"):j.err,j.ok);
    setTimeout(msPoll,800)
  }).catch(function(){msPoll()})
}

function msKey(k){
  if(k==1){
    post("/api/mesh/key",{}).then(function(r){return r.json()}).then(function(j){
      $("ms_key").value=j.key||"";
      if(!j.key)msg("msm2",t("Nessuna chiave salvata"),false)
    });
    return
  }
  dlg({title:t("Creare una chiave nuova?"),
    body:'<p style="margin:0">'+
    esc(t(
      "Le altre schede smettono di capire questa finche non copi la chiave nuova anche su di loro."))+
    '</p>',ok:t("Crea"),danger:true,
    onOk:function(){
      return post("/api/mesh",
        {role:$("ms_role").value,auto:$("ms_auto").classList.contains("on")?1:0,
          newkey:1}).then(function(r){return r.json()}).then(function(j){
            if(j.ok){
              $("ms_key").value=j.key;
              msg("msm2",t("Chiave nuova creata: copiala sulle altre schede"),true)
            }
            return j.ok?"":j.err
          })
    }})
}

function msSave(){
  var k=$("ms_key").value.trim(),
    d={role:$("ms_role").value,auto:$("ms_auto").classList.contains("on")?1:0,ch:$("ms_ch").value};
  if(k&&k!=(MS.key||""))d.key=k;
  post("/api/mesh",d).then(function(r){return r.json()}).then(function(j){
    msg("msm2",j.ok?t("Salvato"):j.err,j.ok);
    if(j.ok){
      MS.key=j.key;
      msPoll()
    }
  })
}

function msSend(c){
  var d={to:$("ms_to").value};
  if(c){
    d.cmd=$("ms_cmd").value.trim();
    if(!d.cmd){
      msg("msm3",t("Scrivi l'azione"),false);
      return
    }
  }else{
    d.text=$("ms_tx").value.trim();
    if(!d.text){
      msg("msm3",t("Scrivi il messaggio"),false);
      return
    }
  }
  post("/api/mesh/send",d).then(function(r){return r.json()}).then(function(j){
    msg("msm3",j.ok?t("Inviato"):j.err,j.ok);
    if(j.ok){
      $("ms_tx").value="";
      setTimeout(msPoll,1500)
    }
  })
}
