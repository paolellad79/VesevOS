/* Caricamento a pezzi: UN SOLO codice per tutti i caricamenti. La pagina di emergenza del firmware usa lo stesso testo (tools/mkpezzi.py lo
   copia in VesevOS/src/interfaces/vos_pezzi_js.h). Il file parte come corpo grezzo a pezzi da 4 KB; la riga dell'indirizzo dice n = nome, t = totale,
   o = byte gia inviati, e = 1 se e l'ultimo pezzo; dopo ogni pezzo la scheda dice a che punto e ({ok,n}). Un record TLS piccolo non chiede alla
   scheda il blocco di RAM da 16 KB che sulla C3 non si trova. Se la rete cade si ripete lo STESSO pezzo (5 volte, pausa di 0,8 s); se la scheda dice
   code "pezzo" (pezzo fuori posto) si riparte dal suo n. Risposta: il JSON dell'ultimo pezzo, uguale a quello dell'invio unico di prima, con s = stato HTTP;
   se la rete non torna: {ok:false,code:"rete",err:errRete}. url puo avere gia una "?". prog(percentuale) e facoltativa. Nella pagina passa da api() (accesso scaduto, spia di connessione). */
function caricaPezzi(url,f,nome,prog,errRete){
  var T=f.size,B=url+(url.indexOf("?")<0?"?":"&")+"n="+encodeURIComponent(nome||f.name)+"&t="+T,Q=typeof api=="function"?api:fetch;
  function pz(o,k){
    var e=Math.min(o+4096,T);
    if(prog)prog(100*e/T|0);
    return Q(B+"&o="+o+"&e="+(e==T?1:0),{method:"POST",body:f.slice(o,e)}).then(function(r){
      return r.json().then(function(j){
        j.s=r.status;
        return j
      })
    }).then(function(j){
      if(j.ok&&e<T)return pz(e,0);
      if(j.code=="pezzo"&&k<5&&j.n<T)return pz(j.n,k+1);
      return j
    },function(x){
      if(x===0||k>=5)return{ok:false,code:"rete",err:errRete};
      return new Promise(function(g){setTimeout(g,800)}).then(function(){return pz(o,k+1)})
    })
  }
  return pz(0,0)
}
