// Mode diffusion : le Cardputer sert lui-même une page web. Le navigateur du PC
// reçoit par WebSocket les lignes de l'écran qui ont changé (compressées) et
// les sons à jouer, avec l'heure du Cardputer pour garder le rythme du Morse.
// Tout le réseau tourne sur le cœur 0, le jeu reste seul sur le cœur 1.
#include "diffusion.h"

#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>

#include <algorithm>

namespace mirror {
namespace {

const char PAGE[] PROGMEM = R"rawliteral(<!doctype html>
<html lang="fr"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Explorer 3</title>
<style>
html,body{margin:0;height:100%;background:#000;overflow:hidden;font-family:sans-serif;color:#eee}
canvas{position:absolute;inset:0;margin:auto;width:min(100vw,177.78vh);height:min(56.25vw,100vh);image-rendering:pixelated;image-rendering:crisp-edges}
#son{position:absolute;inset:0;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:12px;background:rgba(0,0,0,.6);cursor:pointer;font-size:28px;text-align:center}
#son small{font-size:16px;color:#aaa}
#etat{position:absolute;left:12px;bottom:10px;font-size:16px;color:#f80}
#tab{position:absolute;inset:0;display:none;flex-direction:column;align-items:center;justify-content:center;gap:2.5vh;background:#06061a;font-size:2.6vh;text-align:center}
#tab h1{margin:0;color:#ffa028;font-size:4vh;letter-spacing:.08em}
#o2{font:bold 7vh monospace;color:#50c8ff}
#grille{display:grid;grid-template-columns:repeat(3,auto);gap:1.6vh}
.case{display:flex;align-items:center;justify-content:center;gap:3vh;padding:.8vh 4vh;background:#202642;border:2px solid #465a8c;border-radius:1vh}
.case b{font:bold 7vh monospace;color:#ffe146}
.case span{font-size:8vh;line-height:1.1;font-family:"Segoe UI Symbol","DejaVu Sans",sans-serif;color:#ebeef5}
#saisie span{display:inline-block;width:3.5vh;height:3.5vh;margin:0 .6vh;border:2px solid #50ff78;border-radius:.5vh;vertical-align:middle}
#saisie span.on{background:#50ff78}
#tab p{margin:0;color:#8c96af}
#tab.err{animation:err .5s}
@keyframes err{0%,100%{box-shadow:none}40%{box-shadow:inset 0 0 0 2vh #f03c32}}
</style></head><body>
<canvas id="c" width="240" height="135"></canvas>
<div id="tab"><h1>ORDINATEUR DE BORD : TABLE DES SYMBOLES</h1><div id="o2"></div><div id="grille"></div>
<div id="saisie"></div><p>Le joueur du Cardputer vous donne une lettre du code : dites-lui quel symbole lui correspond.</p></div>
<div id="son">Cliquer pour activer le son<small>Double-clic : plein écran</small></div>
<div id="etat">Connexion au Cardputer…</div>
<script>
const cv=document.getElementById('c'),g=cv.getContext('2d'),img=g.createImageData(240,135),px=img.data;
const etat=document.getElementById('etat'),son=document.getElementById('son');
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
son.onclick=()=>{ac=new AudioContext();master=ac.createGain();master.gain.value=.3;master.connect(ac.destination);noise=makeNoise();son.style.display='none';};
document.ondblclick=()=>{document.fullscreenElement?document.exitFullscreen():document.documentElement.requestFullscreen();};

// Table des symboles (ordinateur de bord) : remplace la copie de l'écran.
// Message "K,heure,1,restant_ms,saisis,erreurs,table" ou "K,heure,0".
const SYM=['☺','♥','♦','♣','♠','♂','♀','♪','☼','⌂','▲','‼'];
const tab=document.getElementById('tab'),o2=document.getElementById('o2'),grille=document.getElementById('grille'),saisie=document.getElementById('saisie');
let tabOn=false,tabEnd=0,tabKey='',lastErr=-1;
function panel(a){tabOn=a[2]=='1';tab.style.display=tabOn?'flex':'none';if(!tabOn){lastErr=-1;return;}
 tabEnd=Number(a[1])+off+Number(a[3]);
 if(a[6]!==tabKey){tabKey=a[6];grille.innerHTML='';
  for(let i=0;i<tabKey.length;i+=2){const d=document.createElement('div');d.className='case';
   d.innerHTML='<b>'+tabKey[i]+'</b><span>'+SYM[parseInt(tabKey[i+1],16)]+'︎</span>';grille.appendChild(d);}}
 let h='';for(let i=0;i<4;i++)h+='<span'+(i<Number(a[4])?' class="on"':'')+'></span>';saisie.innerHTML='Saisie '+h;
 const e=Number(a[5]);if(lastErr>=0&&e!==lastErr){tab.classList.remove('err');void tab.offsetWidth;tab.classList.add('err');}lastErr=e;}
setInterval(()=>{if(!tabOn)return;const r=Math.max(0,tabEnd-performance.now()),s=Math.ceil(r/1000);
 o2.textContent='O2 '+String(Math.floor(s/60)).padStart(2,'0')+':'+String(s%60).padStart(2,'0');o2.style.color=r<60000?'#f03c32':'#50c8ff';},100);

function msg(s){const a=s.split(',').map(Number);sync(a[1]);if(s[0]=='K'){panel(s.split(','));return;}if(!ac)return;
 if(s[0]=='T')tone(a[2],a[3],a[4],a[5]);else if(s[0]=='S')stop(a[2],a[3]);else if(s[0]=='R')rumble(a[2]);}
function connect(){const ws=new WebSocket('ws://'+location.hostname+':81/');ws.binaryType='arraybuffer';
 ws.onopen=()=>{etat.textContent='';offs=[];};
 ws.onmessage=e=>{typeof e.data=='string'?msg(e.data):rows(e.data);};
 ws.onclose=()=>{etat.textContent='Connexion au Cardputer…';setTimeout(connect,1000);};}
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

// Page réservée au PC : dernier texte demandé par le jeu (envoyé par la tâche réseau)
portMUX_TYPE panelLock = portMUX_INITIALIZER_UNLOCKED;
char panelText[128] = "0";
volatile bool panelDirty = false;

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
    }
    if (type == WStype_CONNECTED || type == WStype_DISCONNECTED) {
        clients = ws.connectedClients();
    }
}

void netTask(void *) {
    uint32_t nextFrame = 0;
    uint32_t nextPing = 0;
    char txt[64];
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

void startScan() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.scanNetworks(true);
}

bool scanDone(std::vector<WifiNet> &out) {
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) {
        return false;
    }
    out.clear();
    for (int i = 0; i < n; i++) {
        String s = WiFi.SSID(i);
        if (s.isEmpty()) {
            continue;
        }
        auto it = std::find_if(out.begin(), out.end(), [&](const WifiNet &e) { return e.ssid == s; });
        if (it != out.end()) {
            it->rssi = std::max(it->rssi, (int)WiFi.RSSI(i));
        } else {
            out.push_back({s, WiFi.RSSI(i), WiFi.encryptionType(i) == WIFI_AUTH_OPEN});
        }
    }
    WiFi.scanDelete();
    std::sort(out.begin(), out.end(), [](const WifiNet &a, const WifiNet &b) { return a.rssi > b.rssi; });
    return true;
}

void connect(const String &ssid, const String &pass) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.setHostname("explorer3");
    WiFi.begin(ssid.c_str(), pass.c_str());
}

bool connected() {
    return WiFi.status() == WL_CONNECTED;
}

String address() {
    return "http://" + WiFi.localIP().toString();
}

// ---------------------------------------------------------------- serveur

void startServer(const uint16_t *screen, int w, int h) {
    WiFi.setSleep(false);  // sinon le Wi-Fi s'endort entre deux paquets (saccades)
    if (screenMutex) {
        return;  // déjà démarré (changement de Wi-Fi)
    }
    screenBuf = screen;
    screenW = w;
    screenH = std::min(h, MAX_H);
    frameBuf = static_cast<uint8_t *>(malloc(FRAME_BUF));
    screenMutex = xSemaphoreCreateMutex();
    soundQueue = xQueueCreate(64, sizeof(SoundMsg));
    http.on("/", [] { http.send_P(200, "text/html", PAGE); });
    http.onNotFound([] { http.send(404, "text/plain", "404"); });
    http.begin();
    ws.begin();
    ws.onEvent(onWsEvent);
    xTaskCreatePinnedToCore(netTask, "diffusion", 6144, nullptr, 1, nullptr, 0);
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

// ---------------------------------------------------------------- page du PC

void setPanel(const String &text) {
    portENTER_CRITICAL(&panelLock);
    strncpy(panelText, text.c_str(), sizeof(panelText) - 1);  // pas de printf sous verrou
    panelText[sizeof(panelText) - 1] = 0;
    panelDirty = true;
    portEXIT_CRITICAL(&panelLock);
}

}  // namespace mirror
