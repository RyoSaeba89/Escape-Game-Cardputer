// Mode multijoueur : le Cardputer sert lui-même une page web, celle du centre de
// contrôle. Le navigateur (PC, télé, téléphone) reçoit par WebSocket les lignes de
// l'écran qui ont changé (compressées), l'état de sa page (règles, énigmes, table
// du clavier codé) et les sons à jouer, avec l'heure du Cardputer pour garder le
// rythme du Morse.
// Tout le réseau tourne sur le cœur 0, le jeu reste seul sur le cœur 1.
#include "diffusion.h"

#include <ESPmDNS.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_wifi.h>

#include <algorithm>

namespace mirror {
namespace {

const char PAGE[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Explorer 3</title>
<style>
html,body{margin:0;height:100%;background:#000;overflow:hidden;font-family:sans-serif;color:#eee}
#ecran,#son,#tab{position:absolute;top:0;right:0;bottom:0;left:0}
canvas{position:absolute;top:0;right:0;bottom:0;left:0;margin:auto;width:100vw;height:56.25vw;max-width:177.78vh;max-height:100vh;image-rendering:-moz-crisp-edges;image-rendering:pixelated;image-rendering:crisp-edges}
#son{display:flex;flex-direction:column;align-items:center;justify-content:center;background:rgba(0,0,0,.6);cursor:pointer;font-size:28px;text-align:center}
#son small{margin-top:12px;font-size:16px;color:#aaa}
#etat{position:absolute;left:12px;bottom:10px;font-size:16px;color:#f80}
#marge{position:absolute;top:10px;left:0;right:0;display:none;font-size:20px;color:#ffe146;text-align:center}
#band{position:absolute;display:none;overflow:hidden;background:#101429;border:solid #42598c;border-width:1px 0;box-sizing:border-box}
#bandt{position:absolute;left:0;top:0;white-space:nowrap;color:#efeff7;font-family:sans-serif;will-change:transform}
#veille,#veillec{position:absolute;left:0;top:0;width:2px;height:2px;opacity:.01;pointer-events:none}
#tab{display:none;flex-direction:column;align-items:center;justify-content:center;background:#06061a;font-size:3.6vh;text-align:center;padding:0 4vh}
#tab>*+*{margin-top:2vh}
#eq{color:#50c8ff;font-size:2.2vh;letter-spacing:.25em}
#tab h1{margin:0;color:#ffa028;font-size:4vh;letter-spacing:.08em}
#o2{font:bold 6vh monospace;color:#50c8ff}
#o2 sub{font-size:.55em}
#corps{display:flex;flex-direction:column;align-items:center;max-width:135vh}
#corps>*+*{margin-top:2.2vh}
#corps p{margin:0}
#corps .moi{color:#ffe146}
#corps .av{color:#f03c32}
#corps .dim{color:#8c96af}
#corps.grand{font-size:4.6vh}
#grille{display:grid;grid-template-columns:repeat(3,auto);grid-gap:1.6vh;gap:1.6vh}
.case{display:flex;align-items:center;justify-content:center;padding:.8vh 4vh;background:#202642;border:2px solid #465a8c;border-radius:1vh}
.case b{font:bold 7vh monospace;color:#ffe146}
.case svg{display:block;width:7vh;height:7vh;margin-left:3vh;fill:#ebeef5}
#saisie span{display:inline-block;width:3.5vh;height:3.5vh;margin:0 .6vh;border:2px solid #50ff78;border-radius:.5vh;vertical-align:middle}
#saisie span.on{background:#50ff78}
#morse{display:grid;grid-template-columns:repeat(7,auto);grid-gap:1.4vh 3.5vh;gap:1.4vh 3.5vh}
.m{display:flex;align-items:center;font:bold 3.6vh monospace;color:#ebeef5}
.m i{display:inline-block;height:1.3vh;margin-left:.8vh;background:#ffe146;border-radius:.65vh}
.m i.p{width:1.3vh}.m i.t{width:4vh}
#pic{border-collapse:collapse}
#pic td{width:8vh;height:8vh;padding:0;border:2px solid #465a8c;background:#1e2234}
#pic td.on{background:#ffa028}
#pic th{font:bold 3.6vh monospace;color:#ebeef5;padding:.6vh 1.6vh;line-height:1.15}
#pic thead th{vertical-align:bottom}
#pic tbody th{text-align:right}
#pic th.ok{color:#46e164}
#pic td.cur{box-shadow:inset 0 0 0 1vh #ffe146}
#laby{border-collapse:collapse}
#laby td{position:relative;width:22vh;height:9vh;padding:0;border:2px solid #465a8c;background:#1e2234;font-size:2.8vh;text-align:center;vertical-align:middle;white-space:nowrap}
#laby td.tr{background:#8c460f}
#laby td.bad{background:#8c1919}
#laby td.sx{color:#46e164}
#laby td.es{color:#8c96af}
#laby .pl{display:block;width:4.5vh;height:4.5vh;margin:0 auto .4vh}
#laby .as{position:absolute;left:1.2vh;top:.5vh;width:6vh;height:8vh}
#laby .ae{display:block;width:4.5vh;height:6vh;margin:0 auto .4vh}
#hp{width:16vh;height:12vh}
#hp g{stroke:#505a6e}
#hp.on g{stroke:#ffe146}
#tab.err{animation:err .5s}
@keyframes err{0%,100%{box-shadow:none}40%{box-shadow:inset 0 0 0 2vh #f03c32}}
@media (orientation:portrait){
#tab{font-size:4vw;padding:0 4vw}#tab>*+*{margin-top:3vw}#eq{font-size:3.4vw}#tab h1{font-size:5.5vw}#o2{font-size:9vw}
#corps{max-width:none}#corps>*+*{margin-top:3vw}#corps.grand{font-size:5vw}
#grille{grid-gap:2vw;gap:2vw}.case{padding:1vw 4vw}.case b{font-size:9vw}
.case svg{width:9vw;height:9vw;margin-left:3vw}#saisie span{width:4.5vw;height:4.5vw}
#morse{grid-template-columns:repeat(3,auto);grid-gap:2vw 6vw;gap:2vw 6vw}.m{font-size:5vw}
.m i{height:1.8vw;margin-left:1.2vw;border-radius:.9vw}.m i.p{width:1.8vw}.m i.t{width:4.5vw}
#laby td{width:21vw;height:14vw;font-size:3.4vw}#laby .pl{width:5.5vw;height:5.5vw}#laby .as{left:1vw;top:1.5vw;width:5vw;height:7vw}#laby .ae{width:5vw;height:7vw}
#pic td{width:11vw;height:11vw}#pic td.cur{box-shadow:inset 0 0 0 1.4vw #ffe146}#pic th{font-size:5vw;padding:1vw 2vw}#hp{width:24vw;height:18vw}}
</style></head><body>
<div id="ecran">
<canvas id="c" width="240" height="135"></canvas>
<div id="band"><span id="bandt"></span></div>
<div id="tab"><div id="eq"></div><h1 id="titre"></h1><div id="o2"></div><div id="corps"></div></div>
<div id="son"><span id="t-son"></span><small id="t-fs"></small><small id="t-mg"></small></div>
<div id="etat"></div>
</div>
<div id="marge"></div>
<video id="veille" muted loop playsinline src="data:video/webm;base64,GkXfo59ChoEBQveBAULygQRC84EIQoKEd2VibUKHgQJChYECGFOAZwEAAAAAAAHGEU2bdLpNu4tTq4QVSalmU6yBoU27i1OrhBZUrmtTrIHGTbuMU6uEElTDZ1OsggETTbuMU6uEHFO7a1OsggGw7AEAAAAAAABZAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAVSalmoCrXsYMPQkBNgIRMYXZmV0GETGF2ZkSJiECfQAAAAAAAFlSua8iuAQAAAAAAAD/XgQFzxYgAAAAAAAAAAZyBACK1nIN1bmSIgQCGhVZfVlA4g4EBI+ODhDuaygDgkLCBELqBEJqBAlWwhFW5gQESVMNn0HNzzWPAi2PFiAAAAAAAAAABZ8iYRaOHRU5DT0RFUkSHi0xhdmMgbGlidnB4Z8ihRaOIRFVSQVRJT05Eh5MwMDowMDowMi4wMDAwMDAwMDAAH0O2dcPngQCjo4EAAIAQAgCdASoQABAAAEcIhYWImYSIAgIADA1gAP7/q1CAo5mBA+gAsQEAARAQABgAMD/0DAAAAP7/q1CAHFO7a5G7j7OBALeK94EB8YIBaPCBAw=="></video>
<script src="/sym.js"></script>
<script>
const cv=document.getElementById('c'),g=cv.getContext('2d'),img=g.createImageData(240,135),px=img.data;
const etat=document.getElementById('etat'),son=document.getElementById('son');

// Textes de la page dans les deux langues ; le Cardputer envoie la sienne ("L,heure,fr").
const TX={
 fr:{son:'Cliquer ou appuyer sur une touche pour activer le son',fs:'Double-clic, F ou Entrée : plein écran',
  mg:'Flèches haut et bas : marge pour la télé',cx:'Connexion au Cardputer…',mt:'Marge télé : ',
  eq:'CENTRE DE CONTRÔLE',tr:'RÈGLES',r1:'Vous êtes le centre de contrôle, sur Terre.',
  r2:"L'équipage a l'autre partie des indices : parlez-vous !",r3:"Ne montrez pas votre écran à l'équipage.",
  r4:"Le chrono démarre quand l'équipage appuie sur ENTRÉE.",
  t1:'ÉNIGME 1/4 : COFFRE DU FER À SOUDER',
  p1:"Une lettre en Morse clignote sur le cadenas du coffre. L'équipage vous décrit les flashs : trouvez la lettre et dites-la-lui !",
  q1:'Point = flash court · Trait = flash long',
  t2:'ÉNIGME 2/4 : RÉSERVOIRS DE CARBURANT',
  p2:"La porte des réservoirs est verrouillée. Lisez les chiffres à l'équipage et guidez-le. Chaque chiffre = nombre de lumières à la suite, dans l'ordre. Une ligne juste passe au vert.",
  t3:'ÉNIGME 3/4 : SOUTE DES PIÈCES',
  p3:"Le verrou de la soute émet une lettre en Morse. Écoutez, puis décrivez les bips à l'équipage : il a l'alphabet Morse !",
  q3:"Bip court = point · Bip long = trait. L'équipage peut relancer le signal.",
  t4:'ÉNIGME 4/4 : ORDINATEUR DE BORD',
  p4:"Guidez l'équipage jusqu'à la sortie : il doit passer par les 8 planètes dans l'ordre, une case à la fois.",
  h4:'Indice : de la planète la plus proche du Soleil à la plus lointaine.',
  e4:"Mauvaise case : −10 s et retour à l'entrée.",ent:'ENTRÉE',sor:'SORTIE',
  pl:['Mercure','Vénus','Terre','Mars','Jupiter','Saturne','Uranus','Neptune'],
  tc:'ORDINATEUR DE BORD : TABLE DE DÉCODAGE',
  pc:"L'équipage vous donne une lettre du code : décrivez-lui le symbole correspondant.",sa:'Symboles tapés'},
 en:{son:'Click or press a key to enable sound',fs:'Double-click, F or Enter: full screen',
  mg:'Up and down arrows: TV margin',cx:'Connecting to the Cardputer…',mt:'TV margin: ',
  eq:'MISSION CONTROL',tr:'RULES',r1:'You are mission control, on Earth.',
  r2:'The crew has the other half of the clues: talk to each other!',r3:"Don't show your screen to the crew.",
  r4:'The countdown starts when the crew presses ENTER.',
  t1:'PUZZLE 1/4: SOLDERING IRON SAFE',
  p1:"A Morse letter is blinking on the safe's padlock. The crew describes the flashes: find the letter and tell them!",
  q1:'Dot = short flash · Dash = long flash',
  t2:'PUZZLE 2/4: FUEL TANKS',
  p2:'The fuel tank door is locked. Read the numbers to the crew and guide them. Each number = lights in a row, in order. A correct line turns green.',
  t3:'PUZZLE 3/4: PARTS HOLD',
  p3:'The parts hold lock beeps a letter in Morse code. Listen, then describe the beeps to the crew: they have the Morse code!',
  q3:'Short beep = dot · Long beep = dash. The crew can replay the signal.',
  t4:'PUZZLE 4/4: ON-BOARD COMPUTER',
  p4:'Guide the crew to the exit: they must step on the 8 planets in order, one cell at a time.',
  h4:'Hint: from the planet closest to the Sun to the farthest one.',
  e4:'Wrong cell: −10 s and back to the entrance.',ent:'ENTRANCE',sor:'EXIT',
  pl:['Mercury','Venus','Earth','Mars','Jupiter','Saturn','Uranus','Neptune'],
  tc:'ON-BOARD COMPUTER: DECODING TABLE',
  pc:'The crew gives you a letter of the code: describe the matching symbol to them.',sa:'Symbols typed'}};
let lg='';try{lg=localStorage.getItem('lang')||'';}catch(_){}
if(!TX[lg])lg=(navigator.language||'').slice(0,2)=='fr'?'fr':'en';
let connecte=false;
function langue(l){if(!TX[l])return;lg=l;try{localStorage.setItem('lang',l);}catch(_){}
 document.documentElement.lang=l;const t=TX[l];
 for(const k of ['son','fs','mg'])document.getElementById('t-'+k).textContent=t[k];
 if(!connecte)etat.textContent=t.cx;pageVue='';majPage();}
for(let i=3;i<px.length;i+=4)px[i]=255;
let dirty=true;
function put(i,v){px[i]=(v>>8&0xF8)|(v>>13);px[i+1]=(v>>3&0xFC)|(v>>9&3);px[i+2]=(v<<3&0xF8)|(v>>2&7);}
// Lignes : [y][0][240 pixels RGB565] ou [y][1][(nombre, couleur) répétés]
function rows(buf){const b=new Uint8Array(buf);let p=0;
 while(p<b.length){const y=b[p++],m=b[p++],o=y*960;let x=0;
  if(m==0){for(;x<240;x++,p+=2)put(o+x*4,b[p]<<8|b[p+1]);}
  else{while(x<240){const n=b[p],v=b[p+1]<<8|b[p+2];p+=3;for(let k=0;k<n;k++,x++)put(o+x*4,v);}}}
 dirty=true;}
// Bandeau de l'écran de fin ("B,heure,y,h,texte") : posé sur les lignes y à
// y+h-1 de la copie de l'écran (le Cardputer ne les envoie plus) et défilé ici,
// à la vitesse du Cardputer (45 px/s de son écran), pour rester fluide.
const band=document.getElementById('band'),bandt=document.getElementById('bandt');
let bandY=0,bandH=0,bandT0=0,bandL=0,bandW=0,bandHt=0;
function banniere(s){const a=s.split(','),t=a.slice(4).join(',');
 bandY=Number(a[2]);bandH=Number(a[3]);bandt.textContent=t;bandW=0;bandT0=performance.now();
 band.style.display=t?'block':'none';}
function placerBande(){const k=cv.offsetHeight/135,w=cv.offsetWidth,h=Math.round(bandH*k);
 if(w!=bandW||h!=bandHt){bandW=w;bandHt=h;const b=Math.max(1,Math.round(k));
  band.style.left=cv.offsetLeft+'px';band.style.top=Math.round(cv.offsetTop+bandY*k)+'px';
  band.style.width=w+'px';band.style.height=h+'px';band.style.borderWidth=b+'px 0';
  bandt.style.fontSize=Math.round(h*.68)+'px';bandt.style.lineHeight=(h-2*b)+'px';bandL=bandt.offsetWidth+w/2;}
 const x=w-((performance.now()-bandT0)*45*k/1000)%(w+bandL);
 bandt.style.transform='translateX('+x.toFixed(1)+'px)';}
(function draw(){if(dirty){g.putImageData(img,0,0);dirty=false;}
 if(band.style.display=='block')placerBande();requestAnimationFrame(draw);})();

// Son : chaque message donne l'heure du Cardputer, la plus petite différence
// avec l'horloge du PC sert de référence ; les sons sont joués avec LAT ms de marge.
const LAT=150;
let ac=null,master,noise,offs=[],off=0,rumbleAt=null;
const live=[[],[],[],[],[]];
function sync(dev){offs.push(performance.now()-dev);if(offs.length>40)offs.shift();off=Math.min(...offs);}
function when(at){return Math.max(ac.currentTime,ac.currentTime+(at+off+LAT-performance.now())/1000);}
function env(ms){return ms<1500?ms/1500:ms<5000?1:ms<6500?1-(ms-5000)/1500:0;}
function stopCh(ch,t){for(const e of live[ch])if(e.end>t){try{e.o.stop(Math.max(t,e.t));}catch(_){}}}
function track(ch,o,t,end){const e={o,t,end};live[ch].push(e);o.onended=()=>{const a=live[ch],i=a.indexOf(e);if(i>=0)a.splice(i,1);};}
function tone(at,f,d,ch){const t=when(at),len=d/1000;stopCh(ch,t);
 let v=1;if(ch==4)v=(rumbleAt==null?1:env(at-rumbleAt))*70/255;if(v<=0)return;
 const o=ac.createOscillator(),gn=ac.createGain();o.frequency.value=f;
 gn.gain.setValueAtTime(0,t);gn.gain.linearRampToValueAtTime(v,t+.003);
 gn.gain.setValueAtTime(v,t+Math.max(.003,len-.003));gn.gain.linearRampToValueAtTime(0,t+len);
 o.connect(gn).connect(master);o.start(t);o.stop(t+len+.01);track(ch,o,t,t+len+.01);}
function stop(at,ch){const t=when(at);for(let c=0;c<5;c++)if(ch==255||ch==c)stopCh(c,t);}
function rumble(at){const t=when(at),s=ac.createBufferSource(),gn=ac.createGain();
 s.buffer=noise;s.loop=true;rumbleAt=at;
 gn.gain.setValueAtTime(0,t);gn.gain.linearRampToValueAtTime(1,t+1.5);gn.gain.setValueAtTime(1,t+5);gn.gain.linearRampToValueAtTime(0,t+6.5);
 s.connect(gn).connect(master);s.start(t);s.stop(t+6.6);track(3,s,t,t+6.6);}
function makeNoise(){const n=8000,f=400,tmp=new Float32Array(n+f);let v=0,peak=1;
 for(let i=0;i<n+f;i++){v+=(Math.random()*2-1)*.15;v*=.97;if(Math.random()<1/400)v+=(Math.random()*2-1)*.8;tmp[i]=v;peak=Math.max(peak,Math.abs(v));}
 for(let i=0;i<f;i++){const a=i/f;tmp[i]=tmp[i]*a+tmp[n+i]*(1-a);}
 const b=ac.createBuffer(1,n,8000),d=b.getChannelData(0);for(let i=0;i<n;i++)d[i]=tmp[i]/peak*.92;return b;}
function resume(){if(ac&&ac.state=='suspended')ac.resume();}
// Écran toujours allumé : vidéo muette invisible. Firefox et Chrome/Brave ne gardent
// l'écran allumé pour une vidéo muette que si elle vient d'un flux (MediaStream) :
// elle reçoit celui d'un petit canvas redessiné chaque seconde. La WebM reste pour
// les navigateurs sans captureStream. Wake Lock en plus si la page est en HTTPS.
const veille=document.getElementById('veille');
let fluxEssaye=false;
function flux(){fluxEssaye=true;const c=document.createElement('canvas');
 if(!c.captureStream||!('srcObject' in veille))return;
 c.id='veillec';c.width=c.height=16;document.body.appendChild(c);
 const x=c.getContext('2d');let n=0;
 function peindre(){x.fillStyle=n++%2?'#000':'#010101';x.fillRect(0,0,16,16);}
 peindre();
 try{veille.srcObject=c.captureStream();setInterval(peindre,1000);}catch(_){}}
function eveil(){if(!fluxEssaye)flux();const p=veille.play();if(p&&p.catch)p.catch(()=>{});
 if(navigator.wakeLock)navigator.wakeLock.request('screen').catch(()=>{});}
document.addEventListener('visibilitychange',()=>{if(!document.hidden&&ac){resume();eveil();}});
function activer(){if(ac)return;ac=new(window.AudioContext||window.webkitAudioContext)();resume();
 master=ac.createGain();master.gain.value=.3;master.connect(ac.destination);noise=makeNoise();son.style.display='none';eveil();}
son.onclick=activer;
document.addEventListener('click',resume);
function pleinEcran(){const d=document,el=d.documentElement;
 if(d.fullscreenElement||d.mozFullScreenElement||d.webkitFullscreenElement){const f=d.exitFullscreen||d.mozCancelFullScreen||d.webkitExitFullscreen;if(f)f.call(d);return;}
 const f=el.requestFullscreen||el.mozRequestFullScreen||el.webkitRequestFullscreen;if(!f)return;const p=f.call(el);if(p&&p.catch)p.catch(()=>{});}
document.ondblclick=pleinEcran;

// Marge pour la télé (overscan) : 0 à 15 % de chaque côté, ?marge=5 dans l'adresse
// ou flèches haut et bas, gardée par le navigateur.
const ecran=document.getElementById('ecran'),margeTxt=document.getElementById('marge');
let marge=0,margeVue=0;
try{marge=Number(localStorage.getItem('marge'))||0;}catch(_){}
const q=/[?&]marge=(\d+)/.exec(location.search);if(q)marge=Number(q[1]);
function appliquer(){marge=Math.max(0,Math.min(15,marge));ecran.style.transform=marge?'scale('+(1-marge/50)+')':'';}
function changerMarge(d){marge+=d;appliquer();try{localStorage.setItem('marge',marge);}catch(_){}
 margeTxt.textContent=TX[lg].mt+marge+' %';margeTxt.style.display='block';clearTimeout(margeVue);
 margeVue=setTimeout(()=>{margeTxt.style.display='none';},1500);}
appliquer();
// Clavier, télécommande ou manette : une première touche active le son
document.addEventListener('keydown',e=>{if(e.ctrlKey||e.altKey||e.metaKey)return;
 const k=e.key,c=e.keyCode;
 if(!ac){activer();return;}
 resume();
 if(k=='ArrowUp'||k=='Up'||k=='+'||c==38)changerMarge(1);
 else if(k=='ArrowDown'||k=='Down'||k=='-'||c==40)changerMarge(-1);
 else if(k=='f'||k=='F'||k=='Enter'||c==13)pleinEcran();
 else return;
 e.preventDefault();});

// Page du centre de contrôle (multijoueur) : remplace la copie de l'écran.
// Message "K,heure,page,..." : page 0 = copie de l'écran, R = règles, 1 à 4 =
// énigme, C = table du clavier codé ; 1 à 4 et C continuent par
// ",restant_ms,erreurs,données" (voir updatePanel dans main.cpp).
// Symboles en pixel art 12×12 : SYM vient de /sym.js (les dessins de main.cpp),
// 3 chiffres hexadécimaux par ligne.
const SYMS=typeof SYM!='undefined'?SYM:[];
function symbole(n){const h=SYMS[n];if(!h)return '';let d='';
 for(let r=0;r<12;r++){const v=parseInt(h.substr(r*3,3),16);
  for(let c=0;c<12;){if(v>>(11-c)&1){let e=c;while(e<12&&(v>>(11-e)&1))e++;d+='M'+c+' '+r+'h'+(e-c)+'v1h'+(c-e)+'z';c=e;}else c++;}}
 return '<svg viewBox="0 0 12 12" shape-rendering="crispEdges"><path d="'+d+'"/></svg>';}
const MORSE=['.-','-...','-.-.','-..','.','..-.','--.','....','..','.---','-.-','.-..','--','-.','---','.--.','--.-','.-.','...','-','..-','...-','.--','-..-','-.--','--..'];
const tab=document.getElementById('tab'),o2=document.getElementById('o2'),titre=document.getElementById('titre'),corps=document.getElementById('corps');
let page='0',donnees=[],pageVue='',tabEnd=0,lastErr=-1;
function para(txt,cl){return '<p'+(cl?' class="'+cl+'"':'')+'>'+txt+'</p>';}
function alphabet(){let h='<div id="morse">';
 for(let i=0;i<26;i++){h+='<div class="m">'+String.fromCharCode(65+i);
  for(const c of MORSE[i])h+='<i class="'+(c=='.'?'p':'t')+'"></i>';h+='</div>';}
 return h+'</div>';}
// Indices d'une ligne du picross, comme lineClues() du Cardputer
function indices(cases){const out=[];let n=0;for(const c of cases){if(c)n++;else if(n){out.push(n);n=0;}}
 if(n)out.push(n);if(!out.length)out.push(0);return out.join('.');}
// Case du curseur de l'équipage : cur (0 à 24), cadre jaune comme sur le Cardputer
function picross(g,lignes,cols,cur){lignes=lignes.split('/');cols=cols.split('/');cur=Number(cur);
 const on=(r,c)=>g[r*5+c]=='1';let h='<table id="pic"><thead><tr><th></th>';
 for(let c=0;c<5;c++){const col=[0,1,2,3,4].map(r=>on(r,c));
  h+='<th'+(indices(col)==cols[c]?' class="ok"':'')+'>'+cols[c].split('.').join('<br>')+'</th>';}
 h+='</tr></thead><tbody>';
 for(let r=0;r<5;r++){const lig=[0,1,2,3,4].map(c=>on(r,c));
  h+='<tr><th'+(indices(lig)==lignes[r]?' class="ok"':'')+'>'+lignes[r].split('.').join(' ')+'</th>';
  for(let c=0;c<5;c++){const cl=(on(r,c)?'on ':'')+(r*5+c===cur?'cur':'');h+='<td'+(cl?' class="'+cl.trim()+'"':'')+'></td>';}h+='</tr>';}
 return h+'</tbody></table>';}
// Labyrinthe (énigme 4, 4 colonnes x 5 lignes) : planètes en petits dessins SVG, personnage = ASTRO
// de /sym.js (le même dessin que sur le Cardputer)
const PCOUL=['#a0a0a8','#e8c878','#3c78dc','#d2502d','#d8a878','#e6cc8c','#8cdce6','#3c5ad2'];
function planete(k){let h='<svg class="pl" viewBox="0 0 20 20"><circle cx="10" cy="10" r="'+(k==5?5.5:7)+'" fill="'+PCOUL[k]+'"/>';
 if(k==2)h+='<path d="M6 8q3-3 5 0t3 4q-4 2-6-1z" fill="#46b450"/>';
 if(k==4)h+='<path d="M3.5 8h13M3.5 12h13" stroke="#a0643c" stroke-width="1.5"/>';
 if(k==5)h+='<ellipse cx="10" cy="10" rx="9.5" ry="3" fill="none" stroke="#c8b070" stroke-width="1.5"/>';
 return h+'</svg>';}
const ACOUL={W:'#fff',B:'#2858dc',C:'#50c8ff',G:'#969aa0',D:'#50505a',O:'#ffa028'};
function astro(cl){const A=typeof ASTRO!='undefined'?ASTRO:[];let h='<svg class="'+cl+'" viewBox="0 0 9 12" shape-rendering="crispEdges">';
 for(let r=0;r<A.length;r++)for(let c=0;c<A[r].length;c++){const k=ACOUL[A[r][c]];if(k)h+='<rect x="'+c+'" y="'+r+'" width="1" height="1" fill="'+k+'"/>';}
 return h+'</svg>';}
const TRAPPE='<svg class="pl" viewBox="0 0 20 20"><rect x="2" y="2" width="16" height="16" rx="2" fill="#145a28" stroke="#46e164" stroke-width="1.5"/><path d="M7 5v10l7-5z" fill="#46e164"/></svg>';
function labyrinthe(g,pos,trace,bad){const t=TX[lg];pos=Number(pos);bad=Number(bad);let h='<table id="laby">';
 for(let r=0;r<5;r++){h+='<tr>';
  for(let c=0;c<4;c++){const i=r*4+c,v=g[i];let cl=i==bad?'bad':trace[i]=='1'?'tr':'',x;
   if(v=='E'){cl+=' es';x=(i==pos?astro('ae'):'')+t.ent;}else if(v=='X'){cl+=' sx';x=TRAPPE+t.sor;}else{const k=Number(v)-1;x=planete(k)+t.pl[k];}
   h+='<td class="'+cl+'">'+(i==pos&&v!='E'?astro('as'):'')+x+'</td>';}
  h+='</tr>';}
 return h+'</table>';}
const HP='<svg id="hp" viewBox="0 0 64 48"><path d="M6 17h10l14-12v38L16 31H6z" fill="#969aa0"/>'+
 '<g fill="none" stroke-width="4" stroke-linecap="round"><path d="M38 17q5 7 0 14"/><path d="M45 11q10 13 0 26"/><path d="M52 5q15 19 0 38"/></g></svg>';
function majPage(){const t=TX[lg],d=donnees,cle=lg+page+d.join(',');
 if(page=='0'||cle==pageVue)return;pageVue=cle;
 document.getElementById('eq').textContent=t.eq;
 let h='';
 if(page=='R'){titre.textContent=t.tr;h=para(t.r1,'moi')+para(t.r2)+para(t.r3,'av')+para(t.r4,'dim');}
 else if(page=='1'){titre.textContent=t.t1;h=para(t.p1)+para(t.q1,'dim')+alphabet();}
 else if(page=='2'){titre.textContent=t.t2;h=picross(d[0],d[1],d[2],d[3])+para(t.p2);}
 else if(page=='3'){titre.textContent=t.t3;h=HP+para(t.p3)+para(t.q3,'dim');}
 else if(page=='4'){titre.textContent=t.t4;h=labyrinthe(d[0],d[1],d[2],d[3])+para(t.p4)+para(t.h4,'moi')+para(t.e4,'dim');}
 else if(page=='C'){titre.textContent=t.tc;h='<div id="grille">';
  const k=d[1];for(let i=0;i<k.length;i+=2)h+='<div class="case"><b>'+k[i]+'</b>'+symbole(parseInt(k[i+1],16))+'</div>';
  h+='</div><div id="saisie">'+t.sa+' ';for(let i=0;i<4;i++)h+='<span'+(i<Number(d[0])?' class="on"':'')+'></span>';
  h+='</div>'+para(t.pc,'dim');}
 corps.className=page=='R'?'grand':'';corps.innerHTML=h;}
function panel(a){page=a[2];tab.style.display=page=='0'?'none':'flex';
 const chrono=page!='0'&&page!='R';o2.style.display=chrono?'':'none';
 if(!chrono){lastErr=-1;donnees=[];majPage();return;}
 tabEnd=Number(a[1])+off+Number(a[3]);donnees=a.slice(5);majPage();horloge();
 const e=Number(a[4]);if(lastErr>=0&&e!==lastErr){tab.classList.remove('err');void tab.offsetWidth;tab.classList.add('err');}lastErr=e;}
// Énigme 3 : le haut-parleur s'allume pendant les bips du Morse (canal 2)
let bips=[];
function bip(at,d){const t=at+off+LAT;bips.push([t,t+d]);if(bips.length>8)bips.shift();}
function horloge(){if(page=='0'||page=='R')return;const n=performance.now();
 const r=Math.max(0,tabEnd-n),s=Math.ceil(r/1000);
 o2.innerHTML='O<sub>2</sub> '+String(Math.floor(s/60)).padStart(2,'0')+':'+String(s%60).padStart(2,'0');o2.style.color=r<60000?'#f03c32':'#50c8ff';
 const hp=document.getElementById('hp');if(hp)hp.setAttribute('class',bips.some(b=>n>=b[0]&&n<b[1])?'on':'');}
setInterval(horloge,50);
langue(lg);

function msg(s){const a=s.split(',').map(Number);sync(a[1]);
 if(s[0]=='K'){panel(s.split(','));return;}if(s[0]=='B'){banniere(s);return;}if(s[0]=='L'){langue(s.split(',')[2]);return;}
 if(s[0]=='T'&&a[5]==2)bip(a[2],a[4]);else if(s[0]=='S'&&(a[3]==2||a[3]==255))bips=[];
 if(!ac)return;
 if(s[0]=='T')tone(a[2],a[3],a[4],a[5]);else if(s[0]=='S')stop(a[2],a[3]);else if(s[0]=='R')rumble(a[2]);}
function connect(){const ws=new WebSocket('ws://'+location.hostname+':81/');ws.binaryType='arraybuffer';
 ws.onopen=()=>{connecte=true;etat.textContent='';offs=[];};
 ws.onmessage=e=>{typeof e.data=='string'?msg(e.data):rows(e.data);};
 ws.onclose=()=>{connecte=false;etat.textContent=TX[lg].cx;setTimeout(connect,1000);};}
connect();
</script></body></html>)rawliteral";

constexpr uint32_t FRAME_MS = 40;         // 25 images/s au plus
constexpr uint32_t PING_MS = 250;         // heure du Cardputer pour le son
constexpr size_t FRAME_BUF = 12 * 1024;   // une image peut partir en plusieurs fois
constexpr int MAX_H = 135;

struct SoundMsg {
    char kind;  // 'T' note, 'S' arrêt, 'R' grondement
    uint8_t ch;
    uint16_t freq;
    uint16_t dur;
    uint32_t at;
};

WebServer http(80);
WebSocketsServer ws(81);
SemaphoreHandle_t screenMutex = nullptr;
QueueHandle_t soundQueue = nullptr;
const uint16_t *screenBuf = nullptr;
int screenW = 0;
int screenH = 0;
uint32_t rowHash[MAX_H];  // 0 = ligne à renvoyer
int nextRow = 0;          // reprise si une image ne tient pas dans un envoi
uint8_t *frameBuf = nullptr;
volatile int clients = 0;

// Page du centre de contrôle : dernier texte demandé par le jeu
// (envoyé par la tâche réseau)
portMUX_TYPE panelLock = portMUX_INITIALIZER_UNLOCKED;
char panelText[128] = "0";
volatile bool panelDirty = false;

// Bandeau de l'écran de fin : dernier texte demandé par le jeu (envoyé par la
// tâche réseau). Ses lignes sont retirées de la copie de l'écran (bandY, bandH :
// lus et écrits seulement par la tâche réseau).
char bannerText[600] = "";
int bannerY = 0;
int bannerH = 0;
volatile bool bannerDirty = false;
int bandY = 0;
int bandH = 0;

// Langue de la page ("L,heure,fr") et dessins des symboles (/sym.js)
volatile uint8_t pageLang = 0;
volatile bool langDirty = false;
const char *symbolsJs = "const SYM=[];";

// Réseau Explorer3 : portail captif. Le DNS répond l'adresse du Cardputer à
// tous les noms, et le serveur web renvoie vers la page toute adresse qui n'est
// pas la sienne. Les tests de connexion des téléphones (Android, iPhone) et de
// Windows tombent sur la page : le système l'ouvre dans sa fenêtre « Se connecter
// au réseau », qui passe par le Wi-Fi même quand les données mobiles sont allumées.
volatile bool apMode = false;
WiFiUDP dnsUdp;
bool dnsRunning = false;
uint8_t dnsBuf[512];
constexpr uint8_t DNS_TTL_S = 10;  // court : rien ne reste en cache après la partie

// Une question DNS : l'adresse du Cardputer pour une adresse IPv4 (type A),
// une réponse vide pour les autres types. Les paquets mal formés sont ignorés.
void answerDns() {
    int len = dnsUdp.parsePacket();
    if (len <= 0) {
        return;
    }
    if (len > (int)sizeof(dnsBuf)) {
        dnsUdp.flush();  // sinon le paquet resterait en attente et bloquerait les suivants
        return;
    }
    dnsUdp.read(dnsBuf, len);
    // Question seule (pas une réponse), requête standard
    if (len < 12 || (dnsBuf[2] & 0xF8) != 0 || dnsBuf[4] != 0 || dnsBuf[5] != 1) {
        return;
    }
    int p = 12;
    while (p < len && dnsBuf[p] != 0) {
        if (dnsBuf[p] & 0xC0) {
            return;  // pas de nom compressé dans une question
        }
        p += dnsBuf[p] + 1;
    }
    if (p + 5 > len) {
        return;
    }
    bool typeA = dnsBuf[p + 1] == 0 && dnsBuf[p + 2] == 1;
    int n = p + 5;  // fin de la question, la suite (EDNS) n'est pas recopiée
    dnsBuf[2] = 0x84 | (dnsBuf[2] & 0x01);  // réponse, serveur de référence, RD recopié
    dnsBuf[3] = 0;                          // pas d'erreur
    dnsBuf[6] = 0;
    dnsBuf[7] = typeA ? 1 : 0;              // nombre de réponses
    memset(dnsBuf + 8, 0, 4);
    if (typeA) {
        const uint8_t rr[] = {0xC0, 0x0C, 0, 1, 0, 1, 0, 0, 0, DNS_TTL_S, 0, 4};  // nom de la question, A, IN
        memcpy(dnsBuf + n, rr, sizeof(rr));
        n += sizeof(rr);
        IPAddress ip = WiFi.softAPIP();
        for (int i = 0; i < 4; i++) {
            dnsBuf[n++] = ip[i];
        }
    }
    dnsUdp.beginPacket(dnsUdp.remoteIP(), dnsUdp.remotePort());
    dnsUdp.write(dnsBuf, n);
    dnsUdp.endPacket();
}

// Portail captif : true si la requête a été renvoyée vers la page
bool redirectToPage() {
    if (!apMode) {
        return false;
    }
    String host = http.hostHeader();
    int colon = host.indexOf(':');
    if (colon >= 0) {
        host.remove(colon);
    }
    String ip = WiFi.softAPIP().toString();
    if (host == ip || host.equalsIgnoreCase(HOST_NAME)) {
        return false;
    }
    http.sendHeader("Location", "http://" + ip + "/", true);
    http.send(302, "text/plain", "");
    return true;
}

uint32_t hashRow(const uint16_t *p) {
    const uint32_t *w = reinterpret_cast<const uint32_t *>(p);
    uint32_t h = 2166136261u;
    for (int i = 0; i < screenW / 2; i++) {
        h = (h ^ w[i]) * 16777619u;
    }
    return h | 1;
}

// Ligne y en brut ou compressée (répétitions), selon le plus court.
// Renvoie 0 si elle ne tient pas dans la place restante.
size_t encodeRow(int y, uint8_t *out, size_t room) {
    const uint16_t *p = screenBuf + y * screenW;
    size_t rle = 2;
    for (int x = 0; x < screenW;) {
        int n = 1;
        while (x + n < screenW && n < 255 && p[x + n] == p[x]) {
            n++;
        }
        rle += 3;
        x += n;
    }
    size_t raw = 2 + screenW * 2;
    size_t need = std::min(rle, raw);
    if (need > room) {
        return 0;
    }
    out[0] = y;
    if (rle < raw) {
        out[1] = 1;
        uint8_t *o = out + 2;
        for (int x = 0; x < screenW;) {
            int n = 1;
            while (x + n < screenW && n < 255 && p[x + n] == p[x]) {
                n++;
            }
            const uint8_t *b = reinterpret_cast<const uint8_t *>(p + x);
            *o++ = n;
            *o++ = b[0];  // octets dans l'ordre de l'écran (poids fort d'abord)
            *o++ = b[1];
            x += n;
        }
    } else {
        out[1] = 0;
        memcpy(out + 2, p, screenW * 2);
    }
    return need;
}

void sendFrame() {
    size_t len = 0;
    int stopAt = -1;
    xSemaphoreTake(screenMutex, portMAX_DELAY);
    for (int i = 0; i < screenH; i++) {
        int y = (nextRow + i) % screenH;
        if (y >= bandY && y < bandY + bandH) {
            continue;  // bandeau dessiné par la page
        }
        uint32_t h = hashRow(screenBuf + y * screenW);
        if (h == rowHash[y]) {
            continue;
        }
        size_t n = encodeRow(y, frameBuf + len, FRAME_BUF - len);
        if (n == 0) {
            stopAt = y;
            break;
        }
        len += n;
        rowHash[y] = h;
    }
    xSemaphoreGive(screenMutex);
    nextRow = stopAt < 0 ? 0 : stopAt;
    if (len > 0) {
        ws.broadcastBIN(frameBuf, len);
    }
}

void onWsEvent(uint8_t, WStype_t type, uint8_t *, size_t) {
    if (type == WStype_CONNECTED) {
        memset(rowHash, 0, sizeof(rowHash));  // nouveau navigateur : écran complet
        panelDirty = true;
        langDirty = true;
        bannerDirty = true;
    }
    if (type == WStype_CONNECTED || type == WStype_DISCONNECTED) {
        clients = ws.connectedClients();
    }
}

void netTask(void *) {
    uint32_t nextFrame = 0;
    uint32_t nextPing = 0;
    char txt[64];
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);  // serveurs démarrés par startServer()
    for (;;) {
        if (apMode != dnsRunning) {
            dnsRunning = apMode;
            if (dnsRunning) {
                dnsUdp.begin(53);
            } else {
                dnsUdp.stop();
            }
        }
        if (dnsRunning) {
            answerDns();
        }
        http.handleClient();
        ws.loop();
        SoundMsg m;
        while (xQueueReceive(soundQueue, &m, 0) == pdTRUE) {
            if (!clients) {
                continue;
            }
            unsigned long now = millis();
            if (m.kind == 'T') {
                snprintf(txt, sizeof(txt), "T,%lu,%lu,%u,%u,%u", now, (unsigned long)m.at, m.freq, m.dur, m.ch);
            } else if (m.kind == 'S') {
                snprintf(txt, sizeof(txt), "S,%lu,%lu,%u", now, (unsigned long)m.at, m.ch);
            } else {
                snprintf(txt, sizeof(txt), "R,%lu,%lu", now, (unsigned long)m.at);
            }
            ws.broadcastTXT(txt);
        }
        uint32_t now = millis();
        if (langDirty) {
            langDirty = false;
            if (clients) {
                snprintf(txt, sizeof(txt), "L,%lu,%s", (unsigned long)now, pageLang ? "en" : "fr");
                ws.broadcastTXT(txt);
            }
        }
        if (panelDirty) {
            char copy[sizeof(panelText)];
            portENTER_CRITICAL(&panelLock);
            memcpy(copy, panelText, sizeof(copy));
            panelDirty = false;
            portEXIT_CRITICAL(&panelLock);
            if (clients) {
                char out[sizeof(panelText) + 16];
                snprintf(out, sizeof(out), "K,%lu,%s", (unsigned long)now, copy);
                ws.broadcastTXT(out);
            }
        }
        if (bannerDirty) {
            static char copy[sizeof(bannerText)];
            static char out[sizeof(bannerText) + 32];
            portENTER_CRITICAL(&panelLock);
            memcpy(copy, bannerText, sizeof(copy));  // pas de printf sous verrou
            int y = bannerY;
            int h = bannerH;
            bannerDirty = false;
            portEXIT_CRITICAL(&panelLock);
            bool on = copy[0] != 0;
            snprintf(out, sizeof(out), "B,%lu,%d,%d,%s", (unsigned long)now, y, h, copy);
            if (!on) {
                for (int i = bandY; i < bandY + bandH && i < MAX_H; i++) {
                    rowHash[i] = 0;  // lignes du bandeau de nouveau envoyées
                }
            }
            bandY = on ? y : 0;
            bandH = on ? h : 0;
            if (clients) {
                ws.broadcastTXT(out);
            }
        }
        if (clients && (int32_t)(now - nextPing) >= 0) {
            nextPing = now + PING_MS;
            snprintf(txt, sizeof(txt), "P,%lu", (unsigned long)now);
            ws.broadcastTXT(txt);
        }
        if (clients && (int32_t)(now - nextFrame) >= 0) {
            nextFrame = now + FRAME_MS;
            sendFrame();
        }
        vTaskDelay(1);
    }
}

void queueSound(const SoundMsg &m) {
    if (soundQueue) {
        xQueueSend(soundQueue, &m, 0);
    }
}

}  // namespace

// ---------------------------------------------------------------- Wi-Fi

namespace {

// Recherche : passe active (les box répondent à une demande), puis passive
// (écoute des balises, pour celles qui répondent mal). Le pilote refuse de
// chercher pendant une tentative de connexion : chaque passe est retentée.
constexpr int SCAN_TRIES = 5;
constexpr uint32_t SCAN_RETRY_MS = 700;
constexpr uint32_t ACTIVE_MS_PER_CHAN = 400;   // délai de garde de la bibliothèque : × 20
constexpr uint32_t PASSIVE_MS_PER_CHAN = 300;

enum class Pass { Off, Active, Passive };
Pass scanPass = Pass::Off;
bool scanStarted = false;
int scanTries = 0;
uint32_t scanRetryAt = 0;
std::vector<WifiNet> found;

// Connexion : causes données par le pilote (événement « déconnecté »)
volatile uint8_t lastReason = 0;
volatile int disconnects = 0;
volatile int passwordFails = 0;

void onDisconnect(arduino_event_id_t, arduino_event_info_t info) {
    uint8_t r = info.wifi_sta_disconnected.reason;
    if (r == WIFI_REASON_ASSOC_LEAVE) {
        return;  // déconnexion demandée par nous (WiFi.disconnect()), pas un échec
    }
    lastReason = r;
    disconnects++;
    if (r == WIFI_REASON_AUTH_FAIL || r == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT || r == WIFI_REASON_HANDSHAKE_TIMEOUT) {
        passwordFails++;
    }
}

void stationMode() {
    static bool hooked = false;
    if (!hooked) {
        WiFi.onEvent(onDisconnect, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
        hooked = true;
    }
    if (apMode) {
        WiFi.softAPdisconnect(true);
        apMode = false;
    }
    WiFi.setHostname("explorer3");
    WiFi.mode(WIFI_STA);
}

// Arrête une recherche en cours (avant une connexion ou le réseau Explorer3)
void stopScan() {
    if (scanPass != Pass::Off) {
        esp_wifi_scan_stop();
        WiFi.scanDelete();
        scanPass = Pass::Off;
        scanStarted = false;
    }
}

bool launchPass() {
    bool passive = scanPass == Pass::Passive;
    int16_t r = WiFi.scanNetworks(true, false, passive, passive ? PASSIVE_MS_PER_CHAN : ACTIVE_MS_PER_CHAN);
    return r == WIFI_SCAN_RUNNING || r >= 0;
}

// Ajoute les réseaux trouvés par la passe (sans doublons ni réseaux masqués)
void mergeResults(int n) {
    for (int i = 0; i < n; i++) {
        String s = WiFi.SSID(i);
        if (s.isEmpty()) {
            continue;
        }
        auto it = std::find_if(found.begin(), found.end(), [&](const WifiNet &e) { return e.ssid == s; });
        if (it != found.end()) {
            it->rssi = std::max(it->rssi, (int)WiFi.RSSI(i));
        } else {
            found.push_back({s, WiFi.RSSI(i), WiFi.encryptionType(i) == WIFI_AUTH_OPEN});
        }
    }
    std::sort(found.begin(), found.end(), [](const WifiNet &a, const WifiNet &b) { return a.rssi > b.rssi; });
}

// Passe refusée ou trop longue : nouvel essai un peu plus tard
Scan retryPass(std::vector<WifiNet> &out) {
    esp_wifi_scan_stop();
    WiFi.scanDelete();
    if (++scanTries >= SCAN_TRIES) {
        bool second = scanPass == Pass::Passive;
        scanPass = Pass::Off;
        if (second) {
            out = found;  // la première liste reste valable
            return Scan::Done;
        }
        return Scan::Failed;
    }
    WiFi.disconnect();  // arrête une tentative de connexion qui gênerait la recherche
    scanRetryAt = millis() + SCAN_RETRY_MS;
    return Scan::Running;
}

}  // namespace

void startScan() {
    stationMode();
    WiFi.disconnect();
    WiFi.scanDelete();
    found.clear();
    scanPass = Pass::Active;
    scanStarted = false;
    scanTries = 0;
    scanRetryAt = millis() + 100;
}

Scan pollScan(std::vector<WifiNet> &out) {
    if (scanPass == Pass::Off) {
        out = found;
        return Scan::Done;
    }
    if (!scanStarted) {
        if ((int32_t)(millis() - scanRetryAt) < 0) {
            return Scan::Running;
        }
        if (!launchPass()) {
            return retryPass(out);
        }
        scanStarted = true;
        return Scan::Running;
    }
    int16_t n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) {
        return Scan::Running;
    }
    scanStarted = false;
    if (n < 0) {
        return retryPass(out);
    }
    mergeResults(n);
    WiFi.scanDelete();
    out = found;
    if (scanPass == Pass::Active) {
        scanPass = Pass::Passive;
        scanTries = 0;
        scanRetryAt = millis() + 50;
        return Scan::Partial;
    }
    scanPass = Pass::Off;
    return Scan::Done;
}

void connect(const String &ssid, const String &pass) {
    stopScan();
    stationMode();
    WiFi.disconnect();
    lastReason = 0;
    disconnects = 0;
    passwordFails = 0;
    WiFi.setAutoReconnect(true);  // le pilote réessaie de lui-même après un échec
    WiFi.begin(ssid.c_str(), pass.c_str());
}

Link link() {
    if (WiFi.status() == WL_CONNECTED) {
        return Link::Connected;
    }
    return passwordFails >= 2 ? Link::BadPassword : Link::Connecting;
}

Link failure() {
    return lastReason == WIFI_REASON_NO_AP_FOUND ? Link::NotFound : Link::NoAnswer;
}

int attempt() {
    return disconnects + 1;
}

// ---------------------------------------------------------------- réseau Explorer3

bool startAccessPoint() {
    stopScan();
    WiFi.disconnect();
    WiFi.mode(WIFI_AP);
    apMode = WiFi.softAP(AP_SSID, AP_PASS);
    return apMode;
}

bool accessPoint() {
    return apMode;
}

int apClients() {
    return apMode ? WiFi.softAPgetStationNum() : 0;
}

String wifiQrText() {
    return String("WIFI:T:WPA;S:") + AP_SSID + ";P:" + AP_PASS + ";;";
}

String ipAddress() {
    return (apMode ? WiFi.softAPIP() : WiFi.localIP()).toString();
}

// ---------------------------------------------------------------- serveur

namespace {

// Nom explorer3.local (PC, Mac, téléphones ; pas la Ouya), annoncé sur le réseau en place
void announce() {
    MDNS.end();
    if (MDNS.begin("explorer3")) {
        MDNS.addService("http", "tcp", 80);
    }
}

}  // namespace

bool startServer(const uint16_t *screen, int w, int h) {
    WiFi.setSleep(false);  // sinon le Wi-Fi s'endort entre deux paquets (saccades)
    announce();
    if (screenMutex) {
        return true;  // déjà démarré (changement de Wi-Fi)
    }
    // Tout est alloué avant de démarrer quoi que ce soit : en cas de manque de
    // mémoire, rien n'est lancé et un nouvel appel pourra réessayer.
    frameBuf = static_cast<uint8_t *>(malloc(FRAME_BUF));
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    soundQueue = xQueueCreate(64, sizeof(SoundMsg));
    TaskHandle_t task = nullptr;
    if (frameBuf && mutex && soundQueue) {
        xTaskCreatePinnedToCore(netTask, "diffusion", 6144, nullptr, 1, &task, 0);  // attend le signal
    }
    if (!task) {
        free(frameBuf);
        frameBuf = nullptr;
        if (mutex) {
            vSemaphoreDelete(mutex);
        }
        if (soundQueue) {
            vQueueDelete(soundQueue);
            soundQueue = nullptr;
        }
        return false;
    }
    screenBuf = screen;
    screenW = w;
    screenH = std::min(h, MAX_H);
    screenMutex = mutex;
    http.on("/", [] {
        if (!redirectToPage()) {
            http.send_P(200, "text/html", PAGE);
        }
    });
    http.on("/sym.js", [] { http.send(200, "application/javascript", symbolsJs); });
    http.onNotFound([] {
        if (!redirectToPage()) {
            http.send(404, "text/plain", "404");
        }
    });
    http.begin();
    ws.begin();
    ws.onEvent(onWsEvent);
    // Ping toutes les 5 s : un navigateur disparu sans fermer la connexion
    // (Wi-Fi coupé, console éteinte) est déconnecté après 2 pings sans réponse.
    ws.enableHeartbeat(5000, 3000, 2);
    xTaskNotifyGive(task);
    return true;
}

int clientCount() {
    return clients;
}

void lockScreen() {
    if (screenMutex) {
        xSemaphoreTake(screenMutex, portMAX_DELAY);
    }
}

void unlockScreen() {
    if (screenMutex) {
        xSemaphoreGive(screenMutex);
    }
}

// ---------------------------------------------------------------- son

void sendTone(uint32_t at, uint16_t freq, uint16_t dur, uint8_t ch) {
    queueSound({'T', ch, freq, dur, at});
}

void sendStop(uint32_t at, uint8_t ch) {
    queueSound({'S', ch, 0, 0, at});
}

void sendRumble(uint32_t at) {
    queueSound({'R', 0, 0, 0, at});
}

// ---------------------------------------------------------------- page de l'écran

void setPanel(const String &text) {
    portENTER_CRITICAL(&panelLock);
    strncpy(panelText, text.c_str(), sizeof(panelText) - 1);  // pas de printf sous verrou
    panelText[sizeof(panelText) - 1] = 0;
    panelDirty = true;
    portEXIT_CRITICAL(&panelLock);
}

void setBanner(const String &text, int y, int h) {
    portENTER_CRITICAL(&panelLock);
    strncpy(bannerText, text.c_str(), sizeof(bannerText) - 1);
    bannerText[sizeof(bannerText) - 1] = 0;
    bannerY = y;
    bannerH = h;
    bannerDirty = true;
    portEXIT_CRITICAL(&panelLock);
}

void setLanguage(uint8_t lang) {
    pageLang = lang;
    langDirty = true;
}

void setSymbols(const char *js) {
    symbolsJs = js;
}

}  // namespace mirror
