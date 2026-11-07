/* ===== widget della Home definiti dai DRIVER (la microSD e nel suo driver, non nella pagina) =====
   Il firmware manda soli DATI: d = driver, t = titolo, a = testo alternativo, i = icona svg (gia controllata), st = ok|off, v = riga principale, s = riga secondaria.
   Qui si disegna con esc(): i testi del driver non diventano mai codice. L'icona e un'immagine (nessuno script puo girare). */
function widDrv(x){
  var ok=x.st=="ok",alt=x.a||x.t,
    ic='<span class="wic'+(ok?"":" x")+'"'+
    (ok?' aria-hidden="true"':' role="img" aria-label="'+esc(alt)+'"')+'>'+
    apIco({ok:true,icon:x.i})+'</span>',
    sub=x.s?'<div class="ws">'+esc(x.s)+'</div>':"";
  return'<div class="wid wdrv'+(ok?"":" off")+
    '" role="button" tabindex="0" onclick="show(\'io\')" onkeydown="if(event.key==\'Enter\')show(\'io\')" aria-label="'+
    esc(x.t+": "+(x.v||""))+'"><div class="wt">'+ic+esc(x.t)+'</div><div class="wv">'+esc(x.v)+
    '</div>'+sub+'</div>'
}
