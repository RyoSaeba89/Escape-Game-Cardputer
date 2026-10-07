// Mode avec écran : le Cardputer sert lui-même une page web. Le navigateur (PC,
// télé) reçoit par WebSocket les lignes de l'écran qui ont changé (compressées)
// et les sons à jouer, avec l'heure du Cardputer pour garder le rythme du Morse.
// Tout le réseau tourne sur le cœur 0, le jeu reste seul sur le cœur 1.
#include "diffusion.h"

#include <ESPmDNS.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>
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
#veille{position:absolute;left:0;top:0;width:2px;height:2px;opacity:.01;pointer-events:none}
#tab{display:none;flex-direction:column;align-items:center;justify-content:center;background:#06061a;font-size:2.6vh;text-align:center}
#tab>*+*{margin-top:2.5vh}
#tab h1{margin:0;color:#ffa028;font-size:4vh;letter-spacing:.08em}
#o2{font:bold 7vh monospace;color:#50c8ff}
#grille{display:grid;grid-template-columns:repeat(3,auto);grid-gap:1.6vh;gap:1.6vh}
.case{display:flex;align-items:center;justify-content:center;padding:.8vh 4vh;background:#202642;border:2px solid #465a8c;border-radius:1vh}
.case b{font:bold 7vh monospace;color:#ffe146}
.case svg{display:block;width:7vh;height:7vh;margin-left:3vh;fill:#ebeef5}
#saisie span{display:inline-block;width:3.5vh;height:3.5vh;margin:0 .6vh;border:2px solid #50ff78;border-radius:.5vh;vertical-align:middle}
#saisie span.on{background:#50ff78}
#tab p{margin-bottom:0;color:#8c96af}
#tab p.av{color:#f03c32}
#tab.err{animation:err .5s}
@keyframes err{0%,100%{box-shadow:none}40%{box-shadow:inset 0 0 0 2vh #f03c32}}
</style></head><body>
<div id="ecran">
<canvas id="c" width="240" height="135"></canvas>
<div id="tab"><h1 id="t-h1"></h1><div id="o2"></div><div id="grille"></div>
<div id="saisie"></div><p id="t-p"></p><p id="t-av" class="av"></p></div>
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
  mg:'Flèches haut et bas : marge pour la télé',cx:'Connexion au Cardputer…',h1:'ORDINATEUR DE BORD : TABLE DE DÉCODAGE',
  p:'Le joueur du Cardputer vous donne une lettre du code : décrivez-lui le symbole correspondant.',
  av:'Ne montrez pas cet écran au joueur du Cardputer !',sa:'Symboles tapés',mt:'Marge télé : '},
 en:{son:'Click or press a key to enable sound',fs:'Double-click, F or Enter: full screen',
  mg:'Up and down arrows: TV margin',cx:'Connecting to the Cardputer…',h1:'ON-BOARD COMPUTER: DECODING TABLE',
  p:'The Cardputer player gives you a letter of the code: describe the matching symbol to them.',
  av:'Don\'t let the Cardputer player see this screen!',sa:'Symbols typed',mt:'TV margin: '}};
let lg='';try{lg=localStorage.getItem('lang')||'';}catch(_){}
if(!TX[lg])lg=(navigator.language||'').slice(0,2)=='fr'?'fr':'en';
let connecte=false,saisis=0;
function langue(l){if(!TX[l])return;lg=l;try{localStorage.setItem('lang',l);}catch(_){}
 document.documentElement.lang=l;const t=TX[l];
 for(const k of ['son','fs','mg','h1','p','av'])document.getElementById('t-'+k).textContent=t[k];
 if(!connecte)etat.textContent=t.cx;majSaisie();}
for(let i=3;i<px.length;i+=4)px[i]=255;
let dirty=true;
function put(i,v){px[i]=(v>>8&0xF8)|(v>>13);px[i+1]=(v>>3&0xFC)|(v>>9&3);px[i+2]=(v<<3&0xF8)|(v>>2&7);}
// Lignes : [y][0][240 pixels RGB565] ou [y][1][(nombre, couleur) répétés]
function rows(buf){const b=new Uint8Array(buf);let p=0;
 while(p<b.length){const y=b[p++],m=b[p++],o=y*960;let x=0;
  if(m==0){for(;x<240;x++,p+=2)put(o+x*4,b[p]<<8|b[p+1]);}
  else{while(x<240){const n=b[p],v=b[p+1]<<8|b[p+2];p+=3;for(let k=0;k<n;k++,x++)put(o+x*4,v);}}}
 dirty=true;}
(function draw(){if(dirty){g.putImageData(img,0,0);dirty=false;}requestAnimationFrame(draw);})();

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
// Écran toujours allumé : vidéo muette en boucle (Firefox 68 sur la Ouya), Wake Lock si la page est en HTTPS
const veille=document.getElementById('veille');
function eveil(){const p=veille.play();if(p&&p.catch)p.catch(()=>{});
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

// Table des symboles (ordinateur de bord) : remplace la copie de l'écran.
// Message "K,heure,1,restant_ms,saisis,erreurs,table" ou "K,heure,0".
// Symboles en pixel art 12×12 : SYM vient de /sym.js (les dessins de main.cpp),
// 3 chiffres hexadécimaux par ligne.
const SYMS=typeof SYM!='undefined'?SYM:[];
function symbole(n){const h=SYMS[n];if(!h)return '';let d='';
 for(let r=0;r<12;r++){const v=parseInt(h.substr(r*3,3),16);
  for(let c=0;c<12;){if(v>>(11-c)&1){let e=c;while(e<12&&(v>>(11-e)&1))e++;d+='M'+c+' '+r+'h'+(e-c)+'v1h'+(c-e)+'z';c=e;}else c++;}}
 return '<svg viewBox="0 0 12 12" shape-rendering="crispEdges"><path d="'+d+'"/></svg>';}
const tab=document.getElementById('tab'),o2=document.getElementById('o2'),grille=document.getElementById('grille'),saisie=document.getElementById('saisie');
let tabOn=false,tabEnd=0,tabKey='',lastErr=-1;
function majSaisie(){let h='';for(let i=0;i<4;i++)h+='<span'+(i<saisis?' class="on"':'')+'></span>';saisie.innerHTML=TX[lg].sa+' '+h;}
function panel(a){tabOn=a[2]=='1';tab.style.display=tabOn?'flex':'none';if(!tabOn){lastErr=-1;return;}
 tabEnd=Number(a[1])+off+Number(a[3]);
 if(a[6]!==tabKey){tabKey=a[6];grille.innerHTML='';
  for(let i=0;i<tabKey.length;i+=2){const d=document.createElement('div');d.className='case';
   d.innerHTML='<b>'+tabKey[i]+'</b>'+symbole(parseInt(tabKey[i+1],16));grille.appendChild(d);}}
 saisis=Number(a[4]);majSaisie();
 const e=Number(a[5]);if(lastErr>=0&&e!==lastErr){tab.classList.remove('err');void tab.offsetWidth;tab.classList.add('err');}lastErr=e;}
setInterval(()=>{if(!tabOn)return;const r=Math.max(0,tabEnd-performance.now()),s=Math.ceil(r/1000);
 o2.textContent='O2 '+String(Math.floor(s/60)).padStart(2,'0')+':'+String(s%60).padStart(2,'0');o2.style.color=r<60000?'#f03c32':'#50c8ff';},100);
langue(lg);

function msg(s){const a=s.split(',').map(Number);sync(a[1]);
 if(s[0]=='K'){panel(s.split(','));return;}if(s[0]=='L'){langue(s.split(',')[2]);return;}if(!ac)return;
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

// Page réservée à l'équipe de l'écran : dernier texte demandé par le jeu
// (envoyé par la tâche réseau)
portMUX_TYPE panelLock = portMUX_INITIALIZER_UNLOCKED;
char panelText[128] = "0";
volatile bool panelDirty = false;

// Langue de la page ("L,heure,fr") et dessins des symboles (/sym.js)
volatile uint8_t pageLang = 0;
volatile bool langDirty = false;
const char *symbolsJs = "const SYM=[];";

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
bool apMode = false;

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
    http.on("/", [] { http.send_P(200, "text/html", PAGE); });
    http.on("/sym.js", [] { http.send(200, "application/javascript", symbolsJs); });
    http.onNotFound([] { http.send(404, "text/plain", "404"); });
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

void setLanguage(uint8_t lang) {
    pageLang = lang;
    langDirty = true;
}

void setSymbols(const char *js) {
    symbolsJs = js;
}

}  // namespace mirror
