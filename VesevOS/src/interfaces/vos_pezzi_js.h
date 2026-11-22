// SPDX-License-Identifier: GPL-3.0-or-later (licenza commerciale alternativa: vedi COMMERCIAL.md)
// Copyright (C) 2026 Domenico Paolella
// VesevOS - vos_pezzi_js.h
// GENERATO da tools/mkpezzi.py da web/src/js_pezzi.js: non modificare a mano. Il caricamento a pezzi della pagina di emergenza e della pagina vera e
// lo stesso codice (funzione caricaPezzi).
#pragma once

static const char kPezziJs[] = R"JS(function caricaPezzi(url,f,nome,prog,errRete){var T=f.size,B=url+(url.indexOf("?")<0?"?":"&")+"n="+encodeURIComponent(nome||f.name)+"&t="+T,Q=typeof api=="function"?api:fetch;function pz(o,k){var e=Math.min(o+4096,T);if(prog)prog(100*e/T|0);return Q(B+"&o="+o+"&e="+(e==T?1:0),{method:"POST",body:f.slice(o,e)}).then(function(r){return r.json().then(function(j){j.s=r.status;return j})}).then(function(j){if(j.ok&&e<T)return pz(e,0);if(j.code=="pezzo"&&k<5&&j.n<T)return pz(j.n,k+1);return j},function(x){if(x===0||k>=5)return{ok:false,code:"rete",err:errRete};return new Promise(function(g){setTimeout(g,800)}).then(function(){return pz(o,k+1)})})}
return pz(0,0)})JS";
