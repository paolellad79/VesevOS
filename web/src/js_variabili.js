/* ===== Variabili nel messaggio della pagina di accesso: {nome} {scheda} {data} {ora} {fuso}, {{ = una graffa ===== */
function fusoTesto(A){
  var p=function(n){return(n<10?"0":"")+n},
    of=A.to/60,sg=of<0?"-":"+",ao=Math.abs(of),
    tz="UTC"+sg+p(Math.floor(ao/60))+":"+p(ao%60);
  return(A.tn?A.tn+", ":"")+tz
}

/* Data spostata nel fuso della scheda, oppure null se l'ora non e impostata */
function dataSpostata(A){
  if(!A||!A.tv)return null;
  var t0=A.t0!==undefined?A.t0:CLK.t0;
  return new Date((A.te+(Date.now()-t0)/1000+A.to)*1000)
}

/* Sostituisce le variabili; una variabile sconosciuta resta com'e. Il risultato va mostrato SEMPRE con textContent. */
function msgVar(testo,A,d,nome){
  var lg=null;
  var lungo=function(){
    if(!lg)lg=fmtLungo(d,A);
    return lg
  };
  return String(testo||"").replace(/\{\{|\{(nome|scheda|data|ora|fuso)\}/g,function(m,k){
    if(m=="{{")return"{";
    if(k=="nome")return nome||"";
    if(k=="scheda")return AUTH&&AUTH.bd?AUTH.bd:"";
    if(k=="fuso")return A&&A.to!==undefined?fusoTesto(A):"";
    if(!d)return"-";
    return k=="data"?lungo()[1]:lungo()[0]
  })
}
