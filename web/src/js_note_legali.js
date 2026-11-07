/* ===== Note legali ===== */
function lgLoad(){
  loadLic();
  commonLoad().then(function(c){
    var h='<thead><tr><th>'+esc(t("Nome"))+'</th><th>'+esc(t("Versione"))+'</th><th>'+
      esc(t("Licenza"))+'</th></tr></thead><tbody>';
    c.sbom.forEach(function(x){
      h+='<tr><td><a href="'+esc(x.url)+'" target="_blank" rel="noopener">'+esc(x.name)+
        '</a></td><td>'+esc(x.ver)+'</td><td>'+esc(x.lic)+'</td></tr>'
    });
    $("sbom").innerHTML=h+'</tbody>'
  });
  api("/api/region").then(function(r){return r.json()}).then(function(r){
    commonLoad().then(function(){
      var c=COMMON.by[r.country],g=regRule(c);
      $("lgradio").innerHTML='<p>'+
        esc(c?tf("Paese: {0}. Canali 1-{1}, potenza massima {2} dBm EIRP.",cName(c),r.ch,r.limit)
          :t("Paese non scelto: la radio usa le regole piu prudenti (canali 1-11)"))+'</p>'+
        (g?'<p>'+esc(tf("Regole: {0}",g.rule))+'</p>':"")+'<p class="fhint">'+
        esc(t(
          "Il firmware rispetta i limiti del paese scelto. La conformita del prodotto finito (marchi CE, UKCA, FCC...) e responsabilita di chi lo mette in commercio. Non e consulenza legale."))+
        '</p>'
    })
  }).catch(function(){})
}
