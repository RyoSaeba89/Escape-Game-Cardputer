// Escape game "Explorer 3" pour M5Stack Cardputer ADV.
// Le vaisseau s'est écrasé sur Mars : 4 énigmes en 5 minutes pour retrouver
// le code de démarrage "NASA" et redécoller.
//   1. Morse lumineux (N)      -> coffre du fer à souder
//   2. QCM premier rover (A)   -> réservoirs de carburant
//   3. Picross 5x5 (S)         -> stockage des pièces détachées
//   4. Morse sonore (A)        -> ordinateur de bord (alarme O2 coupée)
// Puis saisie de NASA, décollage et écran de fin.
// Fn appuyé 3 fois d'affilée : pause / reprise (maître du jeu).
// Record (O2 restant) gardé en mémoire même après extinction.
// Jeu en français ou en anglais (choix au premier démarrage, textes dans textes.h).
// Au démarrage : « Cardputer seul », ou « Avec écran » (écran et son recopiés
// dans le navigateur d'un PC ou d'une télé, voir diffusion.cpp).
#include <Arduino.h>
#include <M5Cardputer.h>
#include <Preferences.h>
#include <utility/Adafruit_TCA8418/Adafruit_TCA8418.h>

#include "diffusion.h"
#include "textes.h"

#include <algorithm>
#include <memory>
#include <type_traits>
#include <vector>

namespace {

using namespace textes;

constexpr int W = 240;
constexpr int H = 135;

constexpr uint32_t GAME_MS = 5UL * 60UL * 1000UL;
constexpr uint32_t PENALTY_MS = 10000;
constexpr uint32_t FN_GAP_MS = 800;  // délai max entre deux appuis sur Fn
constexpr uint8_t VOLUME = 255;

constexpr uint32_t LAMP_UNIT_MS = 400;   // Morse lumineux (énigme 1)
constexpr uint32_t SOUND_UNIT_MS = 200;  // Morse sonore (énigme 4)
constexpr uint16_t MORSE_FREQ = 700;

// Canaux du haut-parleur
constexpr uint8_t CH_O2 = 0;
constexpr uint8_t CH_SFX = 1;
constexpr uint8_t CH_MORSE = 2;
constexpr uint8_t CH_RUMBLE = 3;
constexpr uint8_t CH_WHISTLE = 4;

constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

constexpr uint16_t C_BLACK = 0x0000;
constexpr uint16_t C_WHITE = 0xFFFF;
constexpr uint16_t C_SPACE = rgb(6, 6, 22);
constexpr uint16_t C_PANEL = rgb(18, 22, 40);
constexpr uint16_t C_PANEL2 = rgb(32, 38, 66);
constexpr uint16_t C_BORDER = rgb(70, 90, 140);
constexpr uint16_t C_TEXT = rgb(235, 238, 245);
constexpr uint16_t C_DIM = rgb(140, 150, 175);
constexpr uint16_t C_ORANGE = rgb(255, 160, 40);
constexpr uint16_t C_YELLOW = rgb(255, 225, 70);
constexpr uint16_t C_RED = rgb(240, 60, 50);
constexpr uint16_t C_GREEN = rgb(70, 225, 100);
constexpr uint16_t C_TERM = rgb(80, 255, 120);
constexpr uint16_t C_CYAN = rgb(80, 200, 255);
constexpr uint16_t C_BLUE = rgb(40, 90, 220);
constexpr uint16_t C_GREY = rgb(150, 150, 160);
constexpr uint16_t C_DGREY = rgb(80, 80, 90);
constexpr uint16_t C_MARS1 = rgb(200, 80, 30);
constexpr uint16_t C_MARS2 = rgb(160, 58, 22);
constexpr uint16_t C_MARS3 = rgb(110, 38, 18);
constexpr uint16_t C_MARS4 = rgb(80, 26, 14);

const char *const MORSE[26] = {
    ".-",   "-...", "-.-.", "-..",  ".",    "..-.", "--.",  "....", "..",
    ".---", "-.-",  ".-..", "--",   "-.",   "---",  ".--.", "--.-", ".-.",
    "...",  "-",    "..-",  "...-", ".--",  "-..-", "-.--", "--..",
};

const char CODE[] = "NASA";

// Vaisseau en pixel art (11 x 18). '.' = transparent.
const char *const ROCKET[] = {
    ".....R.....",
    "....RRR....",
    "...RRRRR...",
    "...WWWWG...",
    "..WWWWWGG..",
    "..WWBBBWG..",
    "..WBCBBBG..",
    "..WBBBBBG..",
    "..WWBBBWG..",
    "..WWWWWWG..",
    "..WWWWWWG..",
    "..WRRRRRG..",
    "..WWWWWWG..",
    "..WWWWWWG..",
    ".RWWWWWWGR.",
    "RRWWWWWWGRR",
    "RR.DDDDD.RR",
    "R...DDD...R",
};
constexpr int ROCKET_W = 11;
constexpr int ROCKET_H = 18;

// Picross : la lettre S
const char *const PICROSS[5] = {
    "#####",
    "#....",
    "#####",
    "....#",
    "#####",
};

enum class St {
    Lang, Mode, WifiList, SsidEntry, Password, Connecting, Address,
    Title, Briefing, Puzzle, Solved, Computer, Launch, Win, GameOver
};

using KeysState = std::decay<decltype(M5Cardputer.Keyboard.keysState())>::type;

// Clavier du Cardputer ADV lu à chaque tour de boucle. Le lecteur de la
// bibliothèque M5Cardputer attend l'interruption du TCA8418 : si une touche
// arrive au mauvais moment, l'interruption est perdue et le clavier ne répond
// plus du tout (le jeu continue de tourner).
class PolledKeyboardReader : public KeyboardReader {
public:
    void begin() override {
        _ok = _tca.begin();
        if (_ok) {
            _tca.matrix(7, 8);
            _tca.flush();
        }
    }

    void update() override {
        if (!_ok) {
            return;
        }
        for (uint8_t ev = _tca.getEvent(); ev != 0; ev = _tca.getEvent()) {
            int code = (ev & 0x7F) - 1;
            int r = code / 10;
            int c = code % 10;
            if (code < 0 || r >= 7 || c >= 8) {
                continue;
            }
            Point2D_t p;  // même disposition que la bibliothèque
            p.x = r * 2 + (c > 3 ? 1 : 0);
            p.y = (c + 4) % 4;
            auto it = std::find(_key_list.begin(), _key_list.end(), p);
            if (ev & 0x80) {
                if (it == _key_list.end()) {
                    _key_list.push_back(p);
                }
            } else if (it != _key_list.end()) {
                _key_list.erase(it);
            }
        }
    }

private:
    Adafruit_TCA8418 _tca;
    bool _ok = false;
};

M5Canvas canvas(&M5Cardputer.Display);

St state = St::Mode;
int puzzle = 0;  // 0..3

// Langue : 0 = français, 1 = anglais (clé NVS "lang", absente au premier démarrage)
uint8_t lang = 0;
int langSel = 0;

const char *tr(Tx &t) {
    return t[lang];
}
uint32_t stateStart = 0;

// Chrono
uint32_t deadline = 0;
bool timerRunning = false;
uint32_t frozenRemaining = GAME_MS;
uint32_t nextO2Beep = 0;

// Effets
uint32_t errFlashUntil = 0;
uint32_t penaltyPopupUntil = 0;
char wrongLetter = 0;
bool helpOpen = false;

// Morse (énigmes 1 et 4)
uint32_t morseStart = 0;
bool morsePlaying = false;
uint32_t morseEnd = 0;

// Picross
bool grid[5][5];
int curX = 0;
int curY = 0;
std::vector<int> rowClues[5];
std::vector<int> colClues[5];

// Ordinateur de bord
String typedCode;
int termShown = 0;
uint32_t codeRefusedUntil = 0;  // « CODE REFUSÉ » affiché jusqu'à cette heure

// Pause (Fn x3)
bool paused = false;
uint32_t pausedAt = 0;
uint32_t pausedRemaining = 0;
int fnCount = 0;
uint32_t lastFn = 0;

// Record : plus grande réserve d'O2 restante
Preferences prefs;
uint32_t bestO2 = 0;
bool newRecord = false;

// Mode « avec écran » : le son part vers le navigateur au lieu du haut-parleur
bool mirrorMode = false;
int modeSel = 0;  // 0 Cardputer seul, 1 avec écran, 2 langue
uint32_t chanUntil[5];  // fin du son en cours sur chaque canal
std::vector<mirror::WifiNet> nets;
bool scanning = false;   // première passe de la recherche en cours
bool refining = false;   // seconde passe en cours (la liste peut encore s'allonger)
bool scanFailed = false;
int netSel = 0;          // 0 = créer Explorer3, 1..n = réseaux, n + 1 = autre réseau
String wifiSsid;
String wifiPass;
String wifiError;
constexpr uint32_t WIFI_TIMEOUT_MS = 20000;
bool qrPage = true;       // écran d'adresse : QR de la page (sinon QR du Wi-Fi Explorer3)
bool apJoined = false;    // un appareil a déjà rejoint Explorer3 (bascule automatique faite)
String symbolsJs;         // dessins des symboles pour la page web (/sym.js)

// Clavier codé de l'ordinateur de bord (mode avec écran) : les touches 1 à 9
// portent des symboles, seule l'équipe de l'écran a la table lettre -> symbole.
// Inspirés des codes Alt ☺ ♥ ♦ ♣ ♠ ♂ ♀ ♪ ☼ ⌂ ▲ ‼ ; la page web reçoit ces mêmes
// dessins (buildSymbolsJs), le rang sert de numéro de symbole dans la table.
constexpr int SYM_COUNT = 12;
const char *const SYMBOLS[SYM_COUNT][12] = {
    {"...XXXXXX...", "..X......X..", ".X........X.", "X..XX..XX..X", "X..XX..XX..X", "X..........X",
     "X..........X", "X.X......X.X", "X..X....X..X", ".X..XXXX..X.", "..X......X..", "...XXXXXX..."},
    {"............", ".XXX....XXX.", "XXXXX..XXXXX", "XXXXXXXXXXXX", "XXXXXXXXXXXX", "XXXXXXXXXXXX",
     ".XXXXXXXXXX.", "..XXXXXXXX..", "...XXXXXX...", "....XXXX....", ".....XX.....", "............"},
    {".....XX.....", "....XXXX....", "...XXXXXX...", "..XXXXXXXX..", ".XXXXXXXXXX.", "XXXXXXXXXXXX",
     "XXXXXXXXXXXX", ".XXXXXXXXXX.", "..XXXXXXXX..", "...XXXXXX...", "....XXXX....", ".....XX....."},
    {"....XXXX....", "...XXXXXX...", "...XXXXXX...", "....XXXX....", ".XX..XX..XX.", "XXXXXXXXXXXX",
     "XXXXXXXXXXXX", ".XX..XX..XX.", ".....XX.....", "....XXXX....", "...XXXXXX...", "............"},
    {".....XX.....", "....XXXX....", "...XXXXXX...", "..XXXXXXXX..", ".XXXXXXXXXX.", "XXXXXXXXXXXX",
     "XXXXXXXXXXXX", ".XXX.XX.XXX.", ".....XX.....", "....XXXX....", "...XXXXXX...", "............"},
    {".......XXXXX", "..........XX", ".........X.X", "........X..X", "..XXXX.X...X", ".X....X.....",
     "X......X....", "X......X....", "X......X....", "X......X....", ".X....X.....", "..XXXX......"},
    {"...XXXXXX...", "..X......X..", ".X........X.", ".X........X.", ".X........X.", "..X......X..",
     "...XXXXXX...", ".....XX.....", "...XXXXXX...", ".....XX.....", ".....XX.....", ".....XX....."},
    {"......XX....", "......XXX...", "......X.XX..", "......X..XX.", "......X...X.", "......X.....",
     "......X.....", "......X.....", "..XXXXX.....", ".XXXXXX.....", ".XXXXXX.....", "..XXXX......"},
    {".....XX.....", ".X...XX...X.", "..X......X..", "....XXXX....", "...X....X...", "XX.X....X.XX",
     "XX.X....X.XX", "...X....X...", "....XXXX....", "..X......X..", ".X...XX...X.", ".....XX....."},
    {".....XX.....", "....X..X....", "...X....X...", "..X......X..", ".X........X.", "X..........X",
     "X..........X", "X..........X", "X..........X", "X..........X", "X..........X", "XXXXXXXXXXXX"},
    {"............", ".....XX.....", ".....XX.....", "....XXXX....", "....XXXX....", "...XXXXXX...",
     "...XXXXXX...", "..XXXXXXXX..", "..XXXXXXXX..", ".XXXXXXXXXX.", ".XXXXXXXXXX.", "............"},
    {"..XX....XX..", "..XX....XX..", "..XX....XX..", "..XX....XX..", "..XX....XX..", "..XX....XX..",
     "..XX....XX..", "............", "............", "..XX....XX..", "..XX....XX..", "............"},
};
int keySym[9];      // symbole de chaque touche 1 à 9
char keyLetter[9];  // lettre que la table du PC donne pour ce symbole
uint32_t errCount = 0;  // le PC fait clignoter sa page à chaque erreur

// Décor
struct Star {
    uint8_t x, y, b;
};
Star stars[45];

// Grondement du décollage : bruit brun bouclable, 1 s à 8 kHz
constexpr int NOISE_LEN = 8000;
constexpr int NOISE_FADE = 400;
int16_t noiseBuf[NOISE_LEN];

// ---------------------------------------------------------------- son

struct Note {
    uint32_t at;
    uint16_t freq;
    uint16_t dur;
    uint8_t ch;
    bool used;
};
Note notes[64];

void schedule(uint32_t at, uint16_t freq, uint16_t dur, uint8_t ch) {
    for (auto &n : notes) {
        if (!n.used) {
            n = {at, freq, dur, ch, true};
            return;
        }
    }
}

void cancelChannel(uint8_t ch) {
    for (auto &n : notes) {
        if (n.used && n.ch == ch) {
            n.used = false;
        }
    }
    if (mirrorMode) {
        mirror::sendStop(millis(), ch);
        chanUntil[ch] = 0;
    } else {
        M5Cardputer.Speaker.stop(ch);
    }
}

void stopAllSound() {
    for (auto &n : notes) {
        n.used = false;
    }
    if (mirrorMode) {
        mirror::sendStop(millis(), 255);
        memset(chanUntil, 0, sizeof(chanUntil));
    } else {
        M5Cardputer.Speaker.stop();
    }
}

void runNotes(uint32_t now) {
    for (auto &n : notes) {
        if (n.used && (int32_t)(now - n.at) >= 0) {
            n.used = false;
            if (mirrorMode) {
                mirror::sendTone(n.at, n.freq, n.dur, n.ch);
                chanUntil[n.ch] = n.at + n.dur;
            } else {
                M5Cardputer.Speaker.tone(n.freq, n.dur, n.ch, true);
            }
        }
    }
}

bool channelPlaying(uint8_t ch) {
    if (mirrorMode) {
        return (int32_t)(chanUntil[ch] - millis()) > 0;
    }
    return M5Cardputer.Speaker.isPlaying(ch);
}

void sfxClick() {
    schedule(millis(), 1200, 25, CH_SFX);
}

void sfxError() {
    uint32_t t = millis();
    schedule(t, 220, 160, CH_SFX);
    schedule(t + 180, 150, 260, CH_SFX);
}

void sfxSuccess() {
    uint32_t t = millis();
    schedule(t, 660, 110, CH_SFX);
    schedule(t + 120, 880, 110, CH_SFX);
    schedule(t + 240, 1320, 220, CH_SFX);
}

void sfxGameOver() {
    uint32_t t = millis();
    schedule(t, 440, 300, CH_SFX);
    schedule(t + 330, 370, 300, CH_SFX);
    schedule(t + 660, 311, 300, CH_SFX);
    schedule(t + 990, 220, 900, CH_SFX);
}

void o2Beeps(uint32_t t) {
    for (int i = 0; i < 3; i++) {
        schedule(t + i * 150, 2000, 70, CH_O2);
    }
}

void buildNoise() {
    float v = 0;
    float peak = 1;
    float *tmp = new float[NOISE_LEN + NOISE_FADE];
    for (int i = 0; i < NOISE_LEN + NOISE_FADE; i++) {
        v += (random(-1000, 1001) / 1000.0f) * 0.15f;
        v *= 0.97f;
        if (random(0, 400) == 0) {
            v += (random(-1000, 1001) / 1000.0f) * 0.8f;  // craquements
        }
        tmp[i] = v;
        if (fabsf(v) > peak) {
            peak = fabsf(v);
        }
    }
    // Fondu enchaîné pour que la boucle n'ait pas de clic
    for (int i = 0; i < NOISE_FADE; i++) {
        float a = (float)i / NOISE_FADE;
        tmp[i] = tmp[i] * a + tmp[NOISE_LEN + i] * (1 - a);
    }
    for (int i = 0; i < NOISE_LEN; i++) {
        noiseBuf[i] = (int16_t)(tmp[i] / peak * 30000.0f);
    }
    delete[] tmp;
}

// ---------------------------------------------------------------- chrono

uint32_t remaining() {
    if (paused) {
        return pausedRemaining;
    }
    if (!timerRunning) {
        return frozenRemaining;
    }
    int32_t r = (int32_t)(deadline - millis());
    return r > 0 ? (uint32_t)r : 0;
}

String fmtTime(uint32_t ms) {
    uint32_t s = (ms + 999) / 1000;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60));
    return String(buf);
}

void penalty() {
    deadline -= PENALTY_MS;
    uint32_t now = millis();
    errFlashUntil = now + 400;
    penaltyPopupUntil = now + 1500;
    errCount++;
    sfxError();
}

// ---------------------------------------------------------------- dessin

void text(const String &s, int x, int y, uint16_t col, int size = 1, textdatum_t datum = TL_DATUM) {
    canvas.setTextSize(size);
    canvas.setTextDatum(datum);
    canvas.setTextColor(col);
    canvas.drawString(s, x, y);
    canvas.setTextSize(1);
}

void shadowText(const String &s, int x, int y, uint16_t col, int size, textdatum_t datum) {
    text(s, x + 1, y + 1, C_BLACK, size, datum);
    text(s, x, y, col, size, datum);
}

int wrapped(const String &s, int x, int y, int w, uint16_t col, int lineH = 14) {
    canvas.setTextSize(1);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextColor(col);
    String line;
    int start = 0;
    int len = s.length();
    while (start <= len) {
        int sp = s.indexOf(' ', start);
        if (sp < 0) {
            sp = len;
        }
        String word = s.substring(start, sp);
        start = sp + 1;
        // Garde « ? », « ! », « : » collés au mot précédent
        while (start < len) {
            int next = s.indexOf(' ', start);
            if (next < 0) {
                next = len;
            }
            String punct = s.substring(start, next);
            if (punct.length() != 1 || strchr("?!:;", punct[0]) == nullptr) {
                break;
            }
            word += " " + punct;
            start = next + 1;
        }
        String cand = line.isEmpty() ? word : line + " " + word;
        if (!line.isEmpty() && canvas.textWidth(cand) > w) {
            canvas.drawString(line, x, y);
            y += lineH;
            line = word;
        } else {
            line = cand;
        }
    }
    if (!line.isEmpty()) {
        canvas.drawString(line, x, y);
        y += lineH;
    }
    return y;
}

bool blink(uint32_t period = 500) {
    return (millis() / period) % 2 == 0;
}

uint16_t rocketColor(char c) {
    switch (c) {
        case 'R': return C_RED;
        case 'W': return C_WHITE;
        case 'G': return C_GREY;
        case 'D': return C_DGREY;
        case 'B': return C_BLUE;
        case 'C': return C_CYAN;
        default: return 0;
    }
}

// lying = couché sur le côté (épave), nez vers la droite
void drawRocket(int x, int y, int s, bool lying = false) {
    for (int r = 0; r < ROCKET_H; r++) {
        for (int c = 0; c < ROCKET_W; c++) {
            char ch = ROCKET[r][c];
            if (ch == '.') {
                continue;
            }
            if (lying) {
                canvas.fillRect(x + (ROCKET_H - 1 - r) * s, y + c * s, s, s, rocketColor(ch));
            } else {
                canvas.fillRect(x + c * s, y + r * s, s, s, rocketColor(ch));
            }
        }
    }
}

void drawFlame(int cx, int top, int s, int len) {
    int f = random(-2, 3);
    canvas.fillTriangle(cx - 3 * s, top, cx + 3 * s, top, cx, top + len + f * s, C_RED);
    canvas.fillTriangle(cx - 2 * s, top, cx + 2 * s, top, cx, top + len * 3 / 4 + f * s, C_ORANGE);
    canvas.fillTriangle(cx - s, top, cx + s, top, cx, top + len / 2, C_YELLOW);
}

void drawStars(int maxY, bool twinkle) {
    uint32_t t = millis() / 300;
    for (int i = 0; i < (int)(sizeof(stars) / sizeof(stars[0])); i++) {
        const Star &s = stars[i];
        if (s.y >= maxY) {
            continue;
        }
        uint8_t b = s.b;
        if (twinkle && ((t + i * 7) % 13) == 0) {
            b = 255;
        }
        canvas.drawPixel(s.x, s.y, rgb(b, b, b));
    }
}

void drawMars(int groundY, bool dark = false) {
    // Ciel : dégradé vers l'horizon poussiéreux
    for (int y = 0; y < groundY; y++) {
        float k = (float)y / groundY;
        uint8_t r = 8 + k * k * (dark ? 70 : 110);
        uint8_t g = 6 + k * k * (dark ? 18 : 40);
        uint8_t b = 20 + k * 10;
        canvas.drawFastHLine(0, y, W, rgb(r, g, b));
    }
    drawStars(groundY - 25, true);
    // Montagnes lointaines
    canvas.fillTriangle(-20, groundY, 40, groundY - 26, 100, groundY, C_MARS4);
    canvas.fillTriangle(60, groundY, 125, groundY - 18, 190, groundY, C_MARS4);
    canvas.fillTriangle(150, groundY, 215, groundY - 30, 280, groundY, C_MARS4);
    canvas.fillTriangle(190, groundY, 215, groundY - 30, 222, groundY - 22, C_MARS3);
    // Sol
    canvas.fillRect(0, groundY, W, H - groundY, C_MARS2);
    canvas.drawFastHLine(0, groundY, W, C_MARS1);
    canvas.fillEllipse(40, groundY + 14, 18, 4, C_MARS3);
    canvas.drawEllipse(40, groundY + 13, 18, 4, C_MARS1);
    canvas.fillEllipse(205, groundY + 22, 14, 3, C_MARS3);
    canvas.fillEllipse(110, groundY + 26, 9, 2, C_MARS3);
    canvas.fillCircle(80, groundY + 6, 3, C_MARS3);
    canvas.fillCircle(180, groundY + 9, 2, C_MARS1);
    canvas.fillCircle(20, groundY + 28, 3, C_MARS1);
}

void drawSmoke(int x, int y, uint32_t now) {
    for (int k = 0; k < 6; k++) {
        int phase = (now / 35 + k * 17) % 100;
        int py = y - phase * 6 / 10;
        int px = x + phase / 6 + (int)(sinf((phase + k * 30) * 0.12f) * 3);
        int r = 2 + phase / 12;
        uint8_t g = 120 - phase;
        canvas.fillCircle(px, py, r, rgb(g, g - 10, g - 15));
    }
}

void drawHud() {
    uint32_t rem = remaining();
    canvas.fillRect(0, 0, W, 15, C_PANEL);
    canvas.drawFastHLine(0, 15, W, C_BORDER);

    text("O2", 3, 2, C_CYAN);
    int bw = 52;
    canvas.drawRect(20, 3, bw + 2, 9, C_DIM);
    int fill = (int)((uint64_t)bw * rem / GAME_MS);
    uint16_t bc = rem > GAME_MS / 2 ? C_GREEN : (rem > GAME_MS / 5 ? C_ORANGE : C_RED);
    canvas.fillRect(21, 4, fill, 7, bc);

    uint16_t tc = C_TEXT;
    if (rem < 60000 && timerRunning) {
        tc = blink(250) ? C_RED : C_ORANGE;
    }
    text(fmtTime(rem), 102, 2, tc, 1, TC_DATUM);
    if ((int32_t)(penaltyPopupUntil - millis()) > 0) {
        text("-10 s", 120, 2, C_RED);
    }

    // Lettres du code trouvées
    int found = puzzle;
    if (state == St::Solved || state == St::Computer || state == St::Launch || state == St::Win) {
        found = puzzle + 1;
    }
    int x = W - 4 * 13 - 2;
    for (int i = 0; i < 4; i++) {
        canvas.fillRect(x + i * 13, 1, 11, 13, i < found ? C_ORANGE : C_PANEL2);
        text(i < found ? String(CODE[i]) : String("?"), x + i * 13 + 6, 2, i < found ? C_BLACK : C_DIM, 1, TC_DATUM);
    }
}

void drawFooter(const String &s) {
    canvas.fillRect(0, H - 14, W, 14, C_PANEL);
    canvas.drawFastHLine(0, H - 15, W, C_BORDER);
    text(s, W / 2, H - 13, C_DIM, 1, TC_DATUM);
}

void drawErrorFlash() {
    if ((int32_t)(errFlashUntil - millis()) > 0) {
        canvas.drawRect(0, 0, W, H, C_RED);
        canvas.drawRect(1, 1, W - 2, H - 2, C_RED);
        canvas.drawRect(2, 2, W - 4, H - 4, C_RED);
    }
}

void drawMorseHelp() {
    canvas.fillRect(0, 16, W, H - 16, C_PANEL);
    text(tr(MORSE_HELP), 4, 18, C_ORANGE);
    text(tr(MORSE_CLOSE), W - 4, 18, C_DIM, 1, TR_DATUM);
    for (int i = 0; i < 26; i++) {
        int col = i / 7;
        int row = i % 7;
        int x = 2 + col * 60;
        int y = 34 + row * 14;
        text(String((char)('A' + i)), x, y, C_TEXT);
        int sx = x + 11;
        for (const char *p = MORSE[i]; *p; p++) {
            if (*p == '.') {
                canvas.fillCircle(sx + 2, y + 6, 2, C_YELLOW);
                sx += 6;
            } else {
                canvas.fillRect(sx, y + 5, 10, 3, C_YELLOW);
                sx += 13;
            }
        }
    }
}

// ---------------------------------------------------------------- Morse

uint32_t morseDuration(const char *code, uint32_t unit) {
    uint32_t d = 0;
    for (const char *p = code; *p; p++) {
        d += (*p == '-' ? 3 : 1) * unit + unit;
    }
    return d;
}

void startMorseLamp() {
    morseStart = millis();
    morseEnd = morseStart + morseDuration(MORSE['N' - 'A'], LAMP_UNIT_MS);
    morsePlaying = true;
}

bool lampOn(uint32_t now) {
    if (!morsePlaying) {
        return false;
    }
    if ((int32_t)(now - morseStart) < 0) {
        return false;
    }
    uint32_t t = now - morseStart;
    for (const char *p = MORSE['N' - 'A']; *p; p++) {
        uint32_t on = (*p == '-' ? 3 : 1) * LAMP_UNIT_MS;
        if (t < on) {
            return true;
        }
        t -= on;
        if (t < LAMP_UNIT_MS) {
            return false;
        }
        t -= LAMP_UNIT_MS;
    }
    return false;
}

void startMorseSound() {
    cancelChannel(CH_MORSE);
    uint32_t t = millis() + 300;
    morseStart = t;
    for (const char *p = MORSE['A' - 'A']; *p; p++) {
        uint32_t on = (*p == '-' ? 3 : 1) * SOUND_UNIT_MS;
        schedule(t, MORSE_FREQ, on, CH_MORSE);
        t += on + SOUND_UNIT_MS;
    }
    morseEnd = t;
    morsePlaying = true;
}

// ---------------------------------------------------------------- picross

std::vector<int> lineClues(const bool cells[5]) {
    std::vector<int> out;
    int run = 0;
    for (int i = 0; i < 5; i++) {
        if (cells[i]) {
            run++;
        } else if (run) {
            out.push_back(run);
            run = 0;
        }
    }
    if (run) {
        out.push_back(run);
    }
    if (out.empty()) {
        out.push_back(0);
    }
    return out;
}

void buildClues() {
    for (int i = 0; i < 5; i++) {
        bool row[5];
        bool col[5];
        for (int j = 0; j < 5; j++) {
            row[j] = PICROSS[i][j] == '#';
            col[j] = PICROSS[j][i] == '#';
        }
        rowClues[i] = lineClues(row);
        colClues[i] = lineClues(col);
    }
}

bool rowOk(int r) {
    return lineClues(grid[r]) == rowClues[r];
}

bool colOk(int c) {
    bool col[5];
    for (int j = 0; j < 5; j++) {
        col[j] = grid[j][c];
    }
    return lineClues(col) == colClues[c];
}

bool picrossSolved() {
    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 5; c++) {
            if (grid[r][c] != (PICROSS[r][c] == '#')) {
                return false;
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------- écrans

// Coupe le texte avec « ... » s'il est plus large que w
String fit(String s, int w) {
    if (canvas.textWidth(s) <= w) {
        return s;
    }
    while (s.length() > 1 && canvas.textWidth(s + "...") > w) {
        s.remove(s.length() - 1);
    }
    return s + "...";
}

void drawScreenTitle(const String &title) {
    canvas.fillScreen(C_SPACE);
    drawStars(H, false);
    canvas.fillRect(0, 0, W, 17, C_PANEL);
    canvas.drawFastHLine(0, 17, W, C_BORDER);
    text(title, 4, 3, C_ORANGE);
}

// Liste d'entrées encadrées (langue, mode) : nom en haut, précision en dessous
void drawChoices(const char *const *names, const char *const *infos, int count, int sel, int top, int h, int gap) {
    for (int i = 0; i < count; i++) {
        int y = top + i * (h + gap);
        bool on = i == sel;
        canvas.fillRoundRect(16, y, W - 32, h, 5, on ? C_PANEL2 : C_PANEL);
        canvas.drawRoundRect(16, y, W - 32, h, 5, on ? C_YELLOW : C_BORDER);
        if (on) {
            canvas.fillTriangle(24, y + h / 2 - 6, 24, y + h / 2 + 6, 31, y + h / 2, C_YELLOW);
        }
        int ty = (h - 26) / 2 + 1;
        text(names[i], 38, y + ty, on ? C_YELLOW : C_TEXT);
        text(infos[i], 38, y + ty + 13, C_DIM);
    }
}

void drawLang() {
    canvas.fillScreen(C_SPACE);
    drawStars(H, true);
    shadowText(tr(LANG_TITLE), W / 2 + 1, 8, C_ORANGE, 1, TC_DATUM);
    const char *names[2] = {LANG_NAMES[0], LANG_NAMES[1]};
    const char *infos[2] = {LANG_INFOS[0], LANG_INFOS[1]};
    drawChoices(names, infos, 2, langSel, 32, 32, 8);
    drawFooter(tr(LANG_FOOTER));
}

void drawMode() {
    canvas.fillScreen(C_SPACE);
    drawStars(H, true);
    shadowText("EXPLORER 3", W / 2 + 1, 4, C_ORANGE, 2, TC_DATUM);
    const char *names[3] = {tr(MODE_SOLO), tr(MODE_SCREEN), tr(MODE_LANG)};
    const char *infos[3] = {tr(MODE_SOLO_INFO), tr(MODE_SCREEN_INFO), tr(MODE_LANG_INFO)};
    drawChoices(names, infos, 3, modeSel, 31, 28, 2);
    drawFooter(tr(MODE_FOOTER));
}

// Petit cadenas 7×8 (réseau protégé)
void drawLock(int x, int y, uint16_t col) {
    canvas.drawRoundRect(x + 1, y, 5, 6, 2, col);
    canvas.fillRect(x, y + 3, 7, 5, col);
}

// 4 barres de signal, de -86 dBm (1 barre) à -60 dBm et plus (4 barres)
void drawSignal(int x, int y, int rssi, uint16_t col) {
    int bars = rssi >= -60 ? 4 : rssi >= -70 ? 3 : rssi >= -78 ? 2 : 1;
    for (int i = 0; i < 4; i++) {
        int h = 3 + i * 2;
        canvas.fillRect(x + i * 4, y + 9 - h, 3, h, i < bars ? col : C_DGREY);
    }
}

int wifiCount() {
    return (int)nets.size() + 2;  // + « créer Explorer3 » et « autre réseau »
}

void drawWifiList() {
    drawScreenTitle(tr(WIFI_TITLE));
    if (scanning || refining) {  // recherche en cours : trois points après le titre
        int x = 8 + canvas.textWidth(tr(WIFI_TITLE));
        for (int i = 0; i < 3; i++) {
            canvas.fillCircle(x + i * 5, 11, 1, (millis() / 250) % 4 > (uint32_t)i ? C_ORANGE : C_PANEL2);
        }
    } else {
        text(tr(WIFI_REFRESH), W - 4, 3, C_DIM, 1, TR_DATUM);
    }
    if (scanning) {
        if (blink()) {
            text(tr(WIFI_SEARCHING), W / 2, 60, C_CYAN, 1, TC_DATUM);
        }
    } else {
        const int rows = 6;
        int count = wifiCount();
        int top = std::max(0, std::min(netSel - rows / 2, count - rows));
        for (int r = 0; r < rows && top + r < count; r++) {
            int i = top + r;
            int y = 20 + r * 16;
            bool sel = i == netSel;
            if (sel) {
                canvas.fillRect(0, y, W, 16, C_PANEL2);
            }
            if (i == 0 || i == count - 1) {
                text(tr(i == 0 ? WIFI_CREATE : WIFI_OTHER), 6, y + 2, sel ? C_YELLOW : C_CYAN);
                continue;
            }
            const mirror::WifiNet &n = nets[i - 1];
            text(fit(n.ssid, 176), 6, y + 2, sel ? C_YELLOW : C_TEXT);
            if (!n.open) {
                drawLock(W - 31, y + 4, C_DIM);
            }
            drawSignal(W - 20, y + 3, n.rssi, sel ? C_YELLOW : C_TEXT);
        }
        if (nets.empty()) {
            text(tr(scanFailed ? WIFI_SCAN_FAILED : WIFI_NONE), W / 2, 66, scanFailed ? C_ORANGE : C_DIM, 1, TC_DATUM);
        }
    }
    if (!wifiError.isEmpty()) {
        canvas.fillRect(0, H - 30, W, 15, C_SPACE);
        text(fit(wifiError, W - 8), W / 2, H - 29, C_RED, 1, TC_DATUM);
    }
    drawFooter(tr(WIFI_FOOTER));
}

// Saisie d'un texte (nom de réseau masqué ou mot de passe), affiché en clair
void drawTyping(const char *title, const String &top, uint16_t topCol, const String &value) {
    drawScreenTitle(title);
    text(fit(top, W - 16), W / 2, 30, topCol, 1, TC_DATUM);
    canvas.fillRoundRect(8, 52, W - 16, 24, 4, C_PANEL);
    canvas.drawRoundRect(8, 52, W - 16, 24, 4, C_YELLOW);
    String shown = value + (blink() ? "_" : " ");
    while (canvas.textWidth(shown) > W - 32 && shown.length() > 1) {
        shown.remove(0, 1);
    }
    text(shown, 16, 58, C_TEXT);
    drawFooter(tr(TYPING_FOOTER));
}

void drawConnecting(uint32_t now) {
    drawScreenTitle(tr(CONNECT_TITLE));
    text(fit(wifiSsid, W - 16), W / 2, 42, C_CYAN, 1, TC_DATUM);
    int dots = (now - stateStart) / 300 % 4;
    for (int i = 0; i < 3; i++) {
        canvas.fillCircle(W / 2 - 16 + i * 16, 70, 3, i < dots ? C_YELLOW : C_DGREY);
    }
    int attempt = mirror::attempt();
    if (attempt > 1) {
        text(String(tr(CONNECT_ATTEMPT)) + String(attempt), W / 2, 86, C_DIM, 1, TC_DATUM);
    }
    drawFooter(tr(CONNECT_FOOTER));
}

// Écran d'adresse : QR code à gauche (99 px : 4 modules de marge blanche pour
// le QR de la page, 2 pour celui du Wi-Fi), textes à droite
void drawAddress() {
    drawScreenTitle(tr(ADDR_TITLE));
    const int tx = 110;
    bool ap = mirror::accessPoint();
    String ip = mirror::ipAddress();
    if (ap && !qrPage) {
        canvas.qrcode(mirror::wifiQrText().c_str(), 4, 20, 99, 1);
        text(tr(ADDR_JOIN), tx, 21, C_TEXT);
        text(mirror::AP_SSID, tx, 36, C_YELLOW);
        text(tr(ADDR_PASSWORD), tx, 53, C_DIM);
        text(mirror::AP_PASS, tx, 67, C_YELLOW);
        text(tr(ADDR_DEVICES) + String(mirror::apClients()), tx, 85, C_DIM);
    } else {
        canvas.qrcode(("http://" + ip).c_str(), 4, 20, 99, 1);
        text(tr(ap ? ADDR_PAGE : ADDR_OPEN), tx, 21, C_TEXT);
        text(ip, tx, 36, C_YELLOW);
        text(mirror::HOST_NAME, tx, 50, C_CYAN);
        if (mirror::clientCount() > 0) {
            text(tr(ADDR_BROWSER_OK), tx, 70, C_GREEN);
        } else {
            wrapped(tr(ADDR_WAITING), tx, 70, W - tx - 2, C_DIM, 13);
        }
    }
    if (blink()) {
        text(tr(ADDR_CONTINUE), tx, 104, C_YELLOW);
    }
    drawFooter(tr(ap ? ADDR_FOOTER_AP : ADDR_FOOTER));
}

void drawTitle(uint32_t now) {
    drawMars(100);
    drawRocket(140, 82, 2, true);
    canvas.fillRect(136, 100, 46, 6, C_MARS2);  // à moitié enfoncé dans le sol
    canvas.fillEllipse(176, 101, 8, 2, C_MARS3);
    drawSmoke(160, 82, now);
    shadowText("EXPLORER 3", W / 2 + 1, 10, C_ORANGE, 3, TC_DATUM);
    shadowText(tr(TITLE_SUB), W / 2, 50, C_TEXT, 1, TC_DATUM);
    if (blink()) {
        shadowText(tr(TITLE_START), W / 2, 118, C_YELLOW, 1, TC_DATUM);
    }
}

void drawBriefing() {
    canvas.fillScreen(C_SPACE);
    drawStars(H, false);
    canvas.fillRoundRect(3, 3, W - 6, H - 22, 5, C_PANEL);
    canvas.drawRoundRect(3, 3, W - 6, H - 22, 5, C_BORDER);
    text(tr(BRIEF_TITLE), 10, 8, C_ORANGE);
    int y = wrapped(tr(BRIEF_TEXT), 10, 24, W - 20, C_TEXT);
    wrapped(tr(BRIEF_RULES), 10, y + 4, W - 20, C_CYAN);
    if (blink()) {
        text(tr(BRIEF_START), W / 2, H - 14, C_YELLOW, 1, TC_DATUM);
    }
}

// Mauvaise réponse encore affichée (« X : ACCÈS REFUSÉ »)
bool wrongShown(uint32_t now) {
    return wrongLetter && (int32_t)(errFlashUntil + 600 - now) > 0;
}

void drawLamp(int cx, int cy, bool on) {
    canvas.fillRoundRect(cx - 32, cy - 32, 64, 64, 6, C_PANEL2);
    canvas.drawRoundRect(cx - 32, cy - 32, 64, 64, 6, C_BORDER);
    for (int i = 0; i < 4; i++) {
        int sx = cx + (i % 2 ? 26 : -26);
        int sy = cy + (i / 2 ? 26 : -26);
        canvas.fillCircle(sx, sy, 2, C_DGREY);
    }
    canvas.fillCircle(cx, cy, 24, C_DGREY);
    if (on) {
        canvas.fillCircle(cx, cy, 22, rgb(255, 140, 20));
        canvas.fillCircle(cx, cy, 17, C_YELLOW);
        canvas.fillCircle(cx, cy, 10, rgb(255, 255, 210));
    } else {
        canvas.fillCircle(cx, cy, 22, rgb(70, 30, 10));
        canvas.fillCircle(cx - 7, cy - 7, 4, rgb(110, 55, 25));
    }
}

void drawSpeaker(int cx, int cy, bool playing) {
    canvas.fillRoundRect(cx - 32, cy - 32, 64, 64, 6, C_PANEL2);
    canvas.drawRoundRect(cx - 32, cy - 32, 64, 64, 6, C_BORDER);
    canvas.fillRect(cx - 18, cy - 7, 10, 14, C_GREY);
    canvas.fillTriangle(cx - 8, cy - 7, cx + 4, cy - 17, cx + 4, cy + 17, C_GREY);
    canvas.fillTriangle(cx - 8, cy + 7, cx - 8, cy - 7, cx + 4, cy + 17, C_GREY);
    if (playing) {
        uint16_t col = channelPlaying(CH_MORSE) ? C_YELLOW : C_DGREY;
        canvas.fillArc(cx + 4, cy, 9, 11, -40, 40, col);
        canvas.fillArc(cx + 4, cy, 16, 18, -40, 40, col);
        canvas.fillArc(cx + 4, cy, 23, 25, -40, 40, col);
    }
}

void drawPuzzleMorse(uint32_t now, bool light) {
    canvas.fillScreen(C_SPACE);
    drawHud();
    if (light) {
        text(tr(P1_TITLE), 4, 19, C_ORANGE);
        wrapped(tr(P1_TEXT), 4, 36, 140, C_TEXT, 13);
        drawLamp(186, 74, lampOn(now));
    } else {
        text(tr(P4_TITLE), 4, 19, C_ORANGE);
        wrapped(tr(P4_TEXT), 4, 36, 140, C_TEXT, 13);
        drawSpeaker(186, 74, morsePlaying);
    }
    if (morsePlaying && (int32_t)(now - morseEnd) >= 0) {
        morsePlaying = false;
    }
    if (wrongShown(now)) {
        text(String(wrongLetter) + tr(DENIED), 4, 105, C_RED);
    }
    drawFooter(tr(morsePlaying ? MORSE_PLAYING : light ? P1_FOOTER : P4_FOOTER));
    if (helpOpen) {
        drawMorseHelp();
    }
}

void drawPuzzleQuiz(uint32_t now) {
    canvas.fillScreen(C_SPACE);
    drawHud();
    text(tr(P2_TITLE), 4, 19, C_ORANGE);
    wrapped(tr(P2_TEXT), 4, 35, W - 8, C_TEXT, 13);
    if (wrongShown(now)) {
        text(String(wrongLetter) + tr(DENIED), 4, 61, C_RED);
    }
    const char *opts[4] = {"Sojourner", "Spirit", "Curiosity", "Perseverance"};
    for (int i = 0; i < 4; i++) {
        int x = 4 + (i % 2) * 118;
        int y = 75 + (i / 2) * 23;
        bool bad = wrongLetter == 'A' + i && wrongShown(now);
        canvas.fillRoundRect(x, y, 114, 21, 4, bad ? rgb(90, 20, 20) : C_PANEL2);
        canvas.drawRoundRect(x, y, 114, 21, 4, bad ? C_RED : C_BORDER);
        canvas.fillRoundRect(x + 3, y + 3, 15, 15, 3, C_ORANGE);
        text(String((char)('A' + i)), x + 11, y + 4, C_BLACK, 1, TC_DATUM);
        text(opts[i], x + 24, y + 4, C_TEXT);
    }
    drawFooter(tr(P2_FOOTER));
}

void drawPuzzlePicross() {
    canvas.fillScreen(C_SPACE);
    drawHud();
    const int cell = 17;
    const int gx = 32;
    const int gy = 47;
    canvas.setFont(&fonts::Font0);
    for (int i = 0; i < 5; i++) {
        // Indices des colonnes (empilés au-dessus)
        uint16_t cc = colOk(i) ? C_GREEN : C_TEXT;
        int n = colClues[i].size();
        for (int k = 0; k < n; k++) {
            text(String(colClues[i][k]), gx + i * cell + cell / 2, gy - 3 - (n - k) * 9, cc, 1, TC_DATUM);
        }
        // Indices des lignes (à gauche)
        uint16_t rc = rowOk(i) ? C_GREEN : C_TEXT;
        String s;
        for (size_t k = 0; k < rowClues[i].size(); k++) {
            s += (k ? " " : "") + String(rowClues[i][k]);
        }
        text(s, gx - 5, gy + i * cell + 5, rc, 1, TR_DATUM);
    }
    canvas.setFont(&fonts::efontJA_12);
    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 5; c++) {
            int x = gx + c * cell;
            int y = gy + r * cell;
            canvas.fillRect(x, y, cell, cell, grid[r][c] ? C_ORANGE : rgb(30, 34, 52));
            canvas.drawRect(x, y, cell + 1, cell + 1, C_BORDER);
        }
    }
    int x = gx + curX * cell;
    int y = gy + curY * cell;
    canvas.drawRect(x - 1, y - 1, cell + 3, cell + 3, C_YELLOW);
    canvas.drawRect(x, y, cell + 1, cell + 1, C_YELLOW);

    const int px = 130;
    text(tr(P3_TITLE), px, 19, C_ORANGE);
    text(tr(P3_PLACE), px, 33, C_TEXT);
    wrapped(tr(P3_TEXT), px, 50, W - px - 4, C_DIM, 13);
    text(tr(P3_MOVE), px, 104, C_CYAN);
    text(tr(P3_LIGHT), px, 118, C_CYAN);
}

void drawSolderingIron(int cx, int cy) {
    canvas.fillRoundRect(cx - 34, cy - 6, 30, 12, 4, C_BLUE);
    canvas.fillRect(cx - 30, cy - 6, 2, 12, C_CYAN);
    canvas.fillRect(cx - 4, cy - 3, 26, 6, C_GREY);
    canvas.fillTriangle(cx + 22, cy - 3, cx + 22, cy + 3, cx + 34, cy, C_ORANGE);
    for (int i = 0; i < 8; i++) {  // câble
        canvas.fillCircle(cx - 36 - i * 2, cy + (int)(sinf(i * 0.6f) * 4), 1, C_DGREY);
    }
}

void drawFuel(int cx, int cy) {
    canvas.fillRoundRect(cx - 18, cy - 20, 36, 42, 5, C_RED);
    canvas.fillRect(cx - 12, cy - 26, 12, 7, C_DGREY);
    canvas.drawRoundRect(cx - 10, cy - 30, 20, 8, 3, C_GREY);
    canvas.fillRect(cx + 6, cy - 25, 6, 6, C_YELLOW);
    canvas.drawLine(cx - 12, cy - 12, cx + 12, cy + 14, rgb(170, 30, 25));
    canvas.drawLine(cx + 12, cy - 12, cx - 12, cy + 14, rgb(170, 30, 25));
    canvas.fillRect(cx - 18, cy + 2, 36, 2, rgb(170, 30, 25));
}

void drawGear(int cx, int cy) {
    for (int i = 0; i < 8; i++) {
        float a = i * PI / 4;
        canvas.fillCircle(cx + cosf(a) * 18, cy + sinf(a) * 18, 5, C_GREY);
    }
    canvas.fillCircle(cx, cy, 17, C_GREY);
    canvas.fillCircle(cx, cy, 7, C_PANEL);
    canvas.fillCircle(cx + 32, cy + 16, 6, C_DGREY);  // petit écrou
    canvas.fillCircle(cx + 32, cy + 16, 2, C_PANEL);
}

void drawSolved() {
    canvas.fillScreen(C_SPACE);
    drawHud();
    canvas.fillRoundRect(3, 19, W - 6, H - 38, 5, C_PANEL);
    canvas.drawRoundRect(3, 19, W - 6, H - 38, 5, C_GREEN);
    text(tr(SOLVED_TITLES[puzzle]), W / 2, 24, C_GREEN, 2, TC_DATUM);
    int ix = 56;
    int iy = 82;
    if (puzzle == 0) {
        drawSolderingIron(ix, iy);
    } else if (puzzle == 1) {
        drawFuel(ix, iy);
    } else {
        drawGear(ix - 6, iy - 4);
    }
    wrapped(tr(SOLVED_ITEMS[puzzle]), 100, 50, W - 108, C_TEXT);
    text(tr(SOLVED_LETTER), 100, 88, C_DIM);
    canvas.fillRoundRect(200, 78, 28, 30, 4, C_ORANGE);
    text(String(CODE[puzzle]), 214 + 1, 82, C_BLACK, 2, TC_DATUM);
    if (blink()) {
        drawFooter(tr(SOLVED_NEXT));
    } else {
        drawFooter("");
    }
}

constexpr uint32_t TERM_STEP = 600;  // une ligne du terminal (TERM_LINES, textes.h) toutes les 600 ms

int termVisible(uint32_t now) {
    int n = (now - stateStart) / TERM_STEP + 1;
    return n > TERM_COUNT ? TERM_COUNT : n;
}

// ---------------------------------------------------------------- clavier codé (diffusion)

void buildKeypad() {
    int syms[SYM_COUNT];
    for (int i = 0; i < SYM_COUNT; i++) {
        syms[i] = i;
    }
    for (int i = SYM_COUNT - 1; i > 0; i--) {
        std::swap(syms[i], syms[random(0, i + 1)]);
    }
    // N, A, S + 6 autres lettres, réparties au hasard sur les touches
    char letters[9] = {'N', 'A', 'S'};
    for (int n = 3; n < 9;) {
        char c = 'A' + random(0, 26);
        if (std::find(letters, letters + n, c) == letters + n) {
            letters[n++] = c;
        }
    }
    for (int i = 8; i > 0; i--) {
        std::swap(letters[i], letters[random(0, i + 1)]);
    }
    for (int k = 0; k < 9; k++) {
        keySym[k] = syms[k];
        keyLetter[k] = letters[k];
    }
}

// Lettres correspondant aux touches tapées
String keypadCode() {
    String s;
    for (unsigned i = 0; i < typedCode.length(); i++) {
        s += keyLetter[typedCode[i] - '1'];
    }
    return s;
}

// Le clavier s'affiche après le texte de l'ordinateur de bord
bool keypadShown(uint32_t now) {
    return now - stateStart >= TERM_COUNT * TERM_STEP + 600;
}

void drawSymbol(int id, int x, int y, int s, uint16_t col) {
    for (int r = 0; r < 12; r++) {
        for (int c = 0; c < 12; c++) {
            if (SYMBOLS[id][r][c] == 'X') {
                canvas.fillRect(x + c * s, y + r * s, s, s, col);
            }
        }
    }
}

// Mauvais code : « CODE REFUSÉ » affiché 1,5 s
bool codeRefused(uint32_t now) {
    return (int32_t)(codeRefusedUntil - now) > 0;
}

// Les mêmes dessins pour la page web (servis en /sym.js) : 3 chiffres
// hexadécimaux par ligne de 12 pixels, bit de poids fort = pixel de gauche.
String buildSymbolsJs() {
    String js = "const SYM=[";
    for (int s = 0; s < SYM_COUNT; s++) {
        js += s ? ",'" : "'";
        for (int r = 0; r < 12; r++) {
            int v = 0;
            for (int c = 0; c < 12; c++) {
                v = v << 1 | (SYMBOLS[s][r][c] == 'X');
            }
            char hex[4];
            snprintf(hex, sizeof(hex), "%03x", v);
            js += hex;
        }
        js += "'";
    }
    return js + "];";
}

void drawKeypad(uint32_t now) {
    text(tr(KEYPAD_TITLE), 4, 20, C_ORANGE);
    if (codeRefused(now)) {
        wrapped(tr(KEYPAD_REFUSED), 4, 36, 88, C_RED, 13);
    } else {
        wrapped(tr(KEYPAD_HINT), 4, 36, 88, C_TERM, 13);
    }
    for (int i = 0; i < 4; i++) {
        int x = 4 + i * 22;
        canvas.drawRect(x, 92, 20, 20, C_TERM);
        if (i < (int)typedCode.length()) {
            drawSymbol(keySym[typedCode[i] - '1'], x + 4, 96, 1, C_TERM);
        } else if (i == (int)typedCode.length() && blink(300)) {
            canvas.fillRect(x + 5, 108, 10, 2, C_TERM);
        }
    }
    for (int k = 0; k < 9; k++) {
        int x = 96 + (k % 3) * 48;
        int y = 18 + (k / 3) * 34;
        canvas.fillRoundRect(x, y, 46, 32, 3, C_PANEL2);
        canvas.drawRoundRect(x, y, 46, 32, 3, C_BORDER);
        text(String(k + 1), x + 5, y + 10, C_DIM);
        drawSymbol(keySym[k], x + 17, y + 4, 2, C_TEXT);
    }
    drawFooter(tr(KEYPAD_FOOTER));
}

// Page du PC : table des symboles pendant l'ordinateur de bord, sinon copie
// de l'écran. Renvoyée à chaque changement et toutes les 500 ms (chrono).
void updatePanel(uint32_t now) {
    static String last;
    static uint32_t lastSent = 0;
    String key = "0";
    if (state == St::Computer && !paused) {
        String table;
        for (char c = 'A'; c <= 'Z'; c++) {
            for (int k = 0; k < 9; k++) {
                if (keyLetter[k] == c) {
                    table += c;
                    table += "0123456789ab"[keySym[k]];
                }
            }
        }
        key = String(typedCode.length()) + "," + String(errCount) + "," + table;
    }
    if (key == last && (key == "0" || now - lastSent < 500)) {
        return;
    }
    last = key;
    lastSent = now;
    mirror::setPanel(key == "0" ? key : "1," + String(remaining()) + "," + key);
}

void drawComputer(uint32_t now) {
    canvas.fillScreen(C_BLACK);
    drawHud();
    if (mirrorMode && keypadShown(now)) {
        drawKeypad(now);
        return;
    }
    int n = termVisible(now);
    for (int i = 0; i < n; i++) {
        uint16_t col = (i == TERM_COUNT - 1) ? C_YELLOW : C_TERM;
        text(tr(TERM_LINES[i]), 4, 19 + i * 13, col);
    }
    if (n == TERM_COUNT) {
        text(tr(CODE_LABEL), 4, 92, C_TERM);
        if (codeRefused(now)) {
            text(tr(CODE_REFUSED), 4, 106, C_RED);
        }
        for (int i = 0; i < 4; i++) {
            int x = 124 + i * 26;
            canvas.drawRect(x, 88, 22, 22, C_TERM);
            if (i < (int)typedCode.length()) {
                text(String(typedCode[i]), x + 12, 92, C_TERM, 1, TC_DATUM);
            } else if (i == (int)typedCode.length() && blink(300)) {
                canvas.fillRect(x + 6, 104, 10, 2, C_TERM);
            }
        }
        drawFooter(tr(CODE_FOOTER));
    }
}

void drawLaunch(uint32_t now) {
    uint32_t t = now - stateStart;
    const int ground = 112;
    const int s = 3;
    int shake = 0;
    float rise = 0;
    if (t < 1500) {
        shake = random(-1, 2);
    } else {
        float k = (t - 1500) / 1000.0f;
        rise = 12 * k * k * k + 8 * k;  // accélération
        shake = random(-1, 2) * (t < 2500 ? 1 : 0);
    }
    // Tremblement de toute la scène
    drawMars(ground);
    int rx = W / 2 - ROCKET_W * s / 2 + shake;
    int ry = ground - ROCKET_H * s - (int)rise;
    int flameLen = t < 1500 ? (int)(t / 1500.0f * 18) : 24 + random(0, 8);
    if (ry + ROCKET_H * s > -40) {
        drawFlame(rx + ROCKET_W * s / 2, ry + ROCKET_H * s, s, flameLen);
    }
    drawRocket(rx, ry, s);
    // Nuages de fumée au sol
    int spread = t < 4000 ? t / 25 : 160;
    for (int i = 0; i < 9; i++) {
        int off = (i - 4) * spread / 6;
        int r = 8 + spread / 14 + (i % 3) * 3;
        uint8_t g = 170 - (i % 3) * 25;
        canvas.fillCircle(W / 2 + off + random(-1, 2), ground + 4 - (i % 2) * 4, r, rgb(g, g - 8, g - 16));
    }
    if (t < 1500) {
        shadowText(tr(LAUNCH_IGNITION), W / 2, 8, C_YELLOW, 1, TC_DATUM);
    } else if (t < 3500) {
        shadowText(tr(LAUNCH_LIFTOFF), W / 2, 8, C_ORANGE, 2, TC_DATUM);
    }
}

void drawWin(uint32_t now) {
    canvas.fillScreen(C_SPACE);
    drawStars(H, true);
    // Mars qui s'éloigne
    canvas.fillCircle(40, 178, 82, C_MARS2);
    canvas.fillCircle(30, 170, 70, C_MARS1);
    canvas.fillEllipse(20, 112, 12, 4, C_MARS3);
    canvas.fillEllipse(70, 124, 8, 3, C_MARS3);
    canvas.fillCircle(52, 104, 3, C_MARS3);
    // La Terre au loin
    canvas.fillCircle(212, 26, 12, C_BLUE);
    canvas.fillEllipse(208, 21, 5, 3, C_GREEN);
    canvas.fillEllipse(216, 31, 4, 3, C_GREEN);
    canvas.fillEllipse(214, 18, 4, 1, C_WHITE);
    // Le vaisseau
    int bob = (int)(sinf(now / 300.0f) * 2);
    int rx = 160;
    int ry = 46 + bob;
    drawFlame(rx + ROCKET_W, ry + ROCKET_H * 2, 2, 16);
    drawRocket(rx, ry, 2);

    shadowText(tr(WIN_TITLE1), 8, 10, C_ORANGE, 2, TL_DATUM);
    shadowText(tr(WIN_TITLE2), 8, 36, C_ORANGE, 2, TL_DATUM);
    shadowText(tr(WIN_TEXT), 8, 62, C_TEXT, 1, TL_DATUM);
    shadowText(tr(WIN_O2) + fmtTime(frozenRemaining), 8, 76, C_CYAN, 1, TL_DATUM);
    if (newRecord) {
        if (blink(300)) {
            shadowText(tr(WIN_NEW_RECORD), 8, 90, C_YELLOW, 1, TL_DATUM);
        }
    } else {
        shadowText(tr(WIN_RECORD) + fmtTime(bestO2), 8, 90, C_DIM, 1, TL_DATUM);
    }
    if (blink()) {
        shadowText(tr(WIN_AGAIN), W - 4, 120, C_YELLOW, 1, TR_DATUM);
    }
}

void drawGameOver(uint32_t now) {
    drawMars(100, true);
    drawRocket(140, 82, 2, true);
    canvas.fillRect(136, 100, 46, 6, C_MARS2);
    canvas.fillEllipse(176, 101, 8, 2, C_MARS3);
    shadowText(tr(LOST_TITLE), W / 2, 14, C_RED, 2, TC_DATUM);
    shadowText(tr(LOST_TEXT1), W / 2, 44, C_TEXT, 1, TC_DATUM);
    shadowText(tr(LOST_TEXT2), W / 2, 58, C_TEXT, 1, TC_DATUM);
    if (blink()) {
        shadowText(tr(LOST_AGAIN), W / 2, 118, C_YELLOW, 1, TC_DATUM);
    }
}

// ---------------------------------------------------------------- logique

void enter(St s) {
    state = s;
    stateStart = millis();
}

void startPuzzle(int p) {
    puzzle = p;
    helpOpen = false;
    wrongLetter = 0;
    morsePlaying = false;
    enter(St::Puzzle);
    if (p == 0) {
        startMorseLamp();
        morseStart += 800;
        morseEnd += 800;
    } else if (p == 2) {
        memset(grid, 0, sizeof(grid));
        curX = curY = 0;
    } else if (p == 3) {
        cancelChannel(CH_O2);  // alarme oxygène coupée pour entendre le Morse
        startMorseSound();
    }
}

void startGame() {
    deadline = millis() + GAME_MS;
    timerRunning = true;
    nextO2Beep = millis() + 1000;
    typedCode = "";
    startPuzzle(0);
}

void stopTimer() {
    frozenRemaining = remaining();
    timerRunning = false;
}

void solvePuzzle() {
    sfxSuccess();
    wrongLetter = 0;
    helpOpen = false;
    cancelChannel(CH_MORSE);
    morsePlaying = false;
    if (puzzle == 3) {
        typedCode = "";
        termShown = 0;
        if (mirrorMode) {
            buildKeypad();
        }
        nextO2Beep = millis() + 1500;
        enter(St::Computer);
    } else {
        enter(St::Solved);
    }
}

void startLaunch() {
    stopTimer();
    newRecord = frozenRemaining > bestO2;
    if (newRecord) {
        bestO2 = frozenRemaining;
        prefs.putUInt("best_o2", bestO2);
    }
    stopAllSound();
    if (mirrorMode) {
        mirror::sendRumble(millis());  // le PC suit la même montée/descente du volume
    } else {
        M5Cardputer.Speaker.setChannelVolume(CH_RUMBLE, 0);
        M5Cardputer.Speaker.playRaw(noiseBuf, NOISE_LEN, 8000, false, 8, CH_RUMBLE, true);
        M5Cardputer.Speaker.setChannelVolume(CH_WHISTLE, 70);
    }
    uint32_t t = millis() + 1200;
    for (int i = 0; i < 40; i++) {
        schedule(t + i * 110, 120 + i * 22, 120, CH_WHISTLE);
    }
    enter(St::Launch);
}

void gameOver() {
    stopTimer();
    frozenRemaining = 0;
    stopAllSound();
    sfxGameOver();
    helpOpen = false;
    enter(St::GameOver);
}

char letterOf(const KeysState &ks) {
    for (char c : ks.word) {
        if (c >= 'a' && c <= 'z') {
            return c - 'a' + 'A';
        }
        if (c >= 'A' && c <= 'Z') {
            return c;
        }
    }
    return 0;
}

bool hasChar(const KeysState &ks, char ch) {
    for (char c : ks.word) {
        if (c == ch) {
            return true;
        }
    }
    return false;
}

void answerLetter(char got, char expected) {
    if (got == expected) {
        solvePuzzle();
    } else {
        wrongLetter = got;
        penalty();
    }
}

void startWifiScan() {
    mirror::startScan();
    scanning = true;
    refining = false;
    scanFailed = false;
    nets.clear();
    netSel = 0;
    enter(St::WifiList);
}

void startWifiConnect() {
    wifiError = "";
    mirror::connect(wifiSsid, wifiPass);
    enter(St::Connecting);
}

// Le serveur web démarre sur le réseau en place (box ou Explorer3)
bool startScreenServer() {
    if (!mirror::startServer(static_cast<const uint16_t *>(canvas.getBuffer()), W, H)) {
        startWifiScan();
        wifiError = tr(ERR_MEMORY);
        return false;
    }
    mirror::setLanguage(lang);
    return true;
}

// Réseau Explorer3 créé par le Cardputer (pas de box) : jamais mémorisé
void startHotspot() {
    wifiError = "";
    if (!mirror::startAccessPoint()) {
        wifiError = tr(ERR_AP);
        return;
    }
    prefs.putBool("auto", false);  // la prochaine fois : liste des Wi-Fi
    if (startScreenServer()) {
        qrPage = false;  // d'abord le QR qui fait rejoindre le Wi-Fi
        apJoined = false;
        enter(St::Address);
    }
}

void chooseLang(uint8_t l) {
    lang = l;
    prefs.putUChar("lang", l);
    mirror::setLanguage(l);
}

// Saisie d'un texte au clavier (nom de réseau, mot de passe) : DEL efface,
// les autres caractères s'ajoutent (ENTRÉE et ` sont traités par l'appelant).
void typeInto(String &s, const KeysState &ks) {
    if (ks.del) {
        if (!s.isEmpty()) {
            s.remove(s.length() - 1);
        }
    } else {
        for (char c : ks.word) {
            s += c;
        }
    }
}

void handleKey(const KeysState &ks) {
    switch (state) {
        case St::Lang:
            if (hasChar(ks, ';') || hasChar(ks, '.')) {
                langSel = 1 - langSel;
                sfxClick();
            } else if (ks.enter) {
                sfxClick();
                chooseLang(langSel);
                enter(St::Mode);
            }
            break;

        case St::Mode:
            if (hasChar(ks, ';')) {
                modeSel = (modeSel + 2) % 3;
                sfxClick();
            } else if (hasChar(ks, '.')) {
                modeSel = (modeSel + 1) % 3;
                sfxClick();
            } else if (ks.enter) {
                sfxClick();
                if (modeSel == 2) {
                    langSel = lang;
                    enter(St::Lang);
                    break;
                }
                mirrorMode = modeSel == 1;
                if (!mirrorMode) {
                    enter(St::Title);
                    break;
                }
                wifiSsid = prefs.getString("ssid", "");
                wifiPass = prefs.getString("pass", "");
                if (wifiSsid.isEmpty() || !prefs.getBool("auto", true)) {
                    startWifiScan();
                } else {
                    startWifiConnect();  // box mémorisée
                }
            }
            break;

        case St::WifiList: {
            int count = wifiCount();
            if (hasChar(ks, '`')) {
                mirrorMode = false;
                wifiError = "";
                enter(St::Mode);
            } else if (scanning) {
                break;
            } else if (hasChar(ks, 'r') || hasChar(ks, 'R')) {
                wifiError = "";
                startWifiScan();
            } else if (hasChar(ks, ';') && netSel > 0) {
                netSel--;
                wifiError = "";  // le message cachait la dernière ligne
            } else if (hasChar(ks, '.') && netSel + 1 < count) {
                netSel++;
                wifiError = "";
            } else if (ks.enter) {
                wifiError = "";
                if (netSel == 0) {
                    startHotspot();
                } else if (netSel == count - 1) {
                    wifiSsid = "";
                    enter(St::SsidEntry);
                } else {
                    const mirror::WifiNet &n = nets[netSel - 1];
                    bool sameNet = n.ssid == prefs.getString("ssid", "");
                    wifiSsid = n.ssid;
                    wifiPass = sameNet ? prefs.getString("pass", "") : String();
                    if (n.open) {
                        startWifiConnect();
                    } else {
                        enter(St::Password);
                    }
                }
            }
            break;
        }

        case St::SsidEntry:
            if (ks.enter) {
                if (!wifiSsid.isEmpty()) {
                    wifiPass = "";
                    enter(St::Password);  // vide pour un réseau ouvert
                }
            } else if (hasChar(ks, '`') && !ks.shift) {
                enter(St::WifiList);
            } else {
                typeInto(wifiSsid, ks);
            }
            break;

        case St::Password:
            if (ks.enter) {
                startWifiConnect();
            } else if (hasChar(ks, '`') && !ks.shift) {
                enter(St::WifiList);
            } else {
                typeInto(wifiPass, ks);
            }
            break;

        case St::Connecting:
            if (hasChar(ks, '`')) {
                startWifiScan();
            }
            break;

        case St::Address:
            if (ks.enter) {
                sfxClick();
                enter(St::Title);
            } else if (ks.tab && mirror::accessPoint()) {
                qrPage = !qrPage;
                sfxClick();
            } else if (hasChar(ks, '`')) {
                startWifiScan();
            }
            break;
        case St::Title:
            if (ks.enter) {
                sfxClick();
                enter(St::Briefing);
            }
            break;

        case St::Briefing:
            if (ks.enter) {
                sfxClick();
                startGame();
            }
            break;

        case St::Puzzle: {
            bool morse = puzzle == 0 || puzzle == 3;
            if (morse && helpOpen) {
                helpOpen = false;  // n'importe quelle touche ferme l'aide
                break;
            }
            if (morse && ks.tab) {
                helpOpen = true;
                break;
            }
            if (morse && ks.space) {
                if (!morsePlaying) {
                    if (puzzle == 0) {
                        startMorseLamp();
                    } else {
                        startMorseSound();
                    }
                }
                break;
            }
            char l = letterOf(ks);
            if (puzzle == 0 && l) {
                answerLetter(l, 'N');
            } else if (puzzle == 1 && l >= 'A' && l <= 'D') {
                answerLetter(l, 'A');
            } else if (puzzle == 3 && l) {
                answerLetter(l, 'A');
            } else if (puzzle == 2) {
                if (hasChar(ks, ';') && curY > 0) curY--;
                if (hasChar(ks, '.') && curY < 4) curY++;
                if (hasChar(ks, ',') && curX > 0) curX--;
                if (hasChar(ks, '/') && curX < 4) curX++;
                if (ks.enter) {
                    grid[curY][curX] = !grid[curY][curX];
                    sfxClick();
                    if (picrossSolved()) {
                        solvePuzzle();
                    }
                }
            }
            break;
        }

        case St::Solved:
            if (ks.enter) {
                sfxClick();
                startPuzzle(puzzle + 1);
            }
            break;

        case St::Computer: {
            if (mirrorMode) {
                // Clavier codé : typedCode garde les touches 1 à 9
                if (!keypadShown(millis())) {
                    break;
                }
                char d = 0;
                for (char c : ks.word) {
                    if (c >= '1' && c <= '9') {
                        d = c;
                    }
                }
                if (d && typedCode.length() < 4) {
                    typedCode += d;
                    sfxClick();
                } else if (ks.del && typedCode.length() > 0) {
                    typedCode.remove(typedCode.length() - 1);
                } else if (ks.enter && typedCode.length() == 4) {
                    if (keypadCode() == CODE) {
                        startLaunch();
                    } else {
                        typedCode = "";
                        codeRefusedUntil = millis() + 1500;
                        penalty();
                    }
                }
                break;
            }
            if (termVisible(millis()) < TERM_COUNT) {
                break;
            }
            char l = letterOf(ks);
            if (l && typedCode.length() < 4) {
                typedCode += l;
                sfxClick();
            } else if (ks.del && typedCode.length() > 0) {
                typedCode.remove(typedCode.length() - 1);
            } else if (ks.enter && typedCode.length() == 4) {
                if (typedCode == CODE) {
                    startLaunch();
                } else {
                    typedCode = "";
                    codeRefusedUntil = millis() + 1500;
                    penalty();
                }
            }
            break;
        }

        case St::Win:
        case St::GameOver:
            if (ks.enter) {
                stopAllSound();
                enter(St::Title);
            }
            break;

        default:
            break;
    }
}

void update(uint32_t now) {
    if (state == St::WifiList && (scanning || refining)) {
        std::vector<mirror::WifiNet> found;
        mirror::Scan r = mirror::pollScan(found);
        if (r == mirror::Scan::Partial || r == mirror::Scan::Done) {
            // Garder la sélection sur le même réseau quand la liste se complète
            String sel = netSel > 0 && netSel <= (int)nets.size() ? nets[netSel - 1].ssid : String();
            nets = found;
            if (scanning) {
                netSel = nets.empty() ? 0 : 1;
            } else if (!sel.isEmpty()) {
                for (size_t i = 0; i < nets.size(); i++) {
                    if (nets[i].ssid == sel) {
                        netSel = i + 1;
                    }
                }
            }
            netSel = std::min(netSel, wifiCount() - 1);
            scanning = false;
            refining = r == mirror::Scan::Partial;
        } else if (r == mirror::Scan::Failed) {
            scanFailed = nets.empty();
            scanning = refining = false;
        }
    }
    if (state == St::Connecting) {
        mirror::Link link = mirror::link();
        if (link == mirror::Link::Connected) {
            prefs.putString("ssid", wifiSsid);
            prefs.putString("pass", wifiPass);
            prefs.putBool("auto", true);
            if (startScreenServer()) {
                qrPage = true;
                enter(St::Address);
            }
        } else if (link == mirror::Link::BadPassword) {
            startWifiScan();
            wifiError = tr(ERR_PASSWORD);
        } else if (now - stateStart > WIFI_TIMEOUT_MS) {
            bool notFound = mirror::failure() == mirror::Link::NotFound;
            startWifiScan();
            wifiError = tr(notFound ? ERR_NOT_FOUND : ERR_NO_ANSWER);
        }
    }
    if (state == St::Address && mirror::accessPoint() && !apJoined && mirror::apClients() > 0) {
        apJoined = true;  // un appareil a rejoint Explorer3 : QR de la page
        qrPage = true;
    }

    bool playing = state == St::Puzzle || state == St::Solved || state == St::Computer;
    if (playing && timerRunning) {
        if (remaining() == 0) {
            gameOver();
            return;
        }
        // Alarme oxygène, de plus en plus rapide (muette pendant les énigmes 1 et 4)
        bool mute = state == St::Puzzle && (puzzle == 0 || puzzle == 3);
        if (!mute && (int32_t)(now - nextO2Beep) >= 0) {
            o2Beeps(now);
            uint32_t rem = remaining();
            nextO2Beep = now + 1500 + (uint32_t)((uint64_t)8500 * rem / GAME_MS);
        } else if (mute) {
            nextO2Beep = now + 1500;
        }
    }

    if (state == St::Computer) {
        int n = termVisible(now);
        if (n != termShown) {
            termShown = n;
            schedule(now, n == TERM_COUNT ? 1500 : 900, 40, CH_SFX);
        }
    }

    if (state == St::Launch) {
        uint32_t t = now - stateStart;
        int vol;
        if (t < 1500) {
            vol = t * 255 / 1500;
        } else if (t < 5000) {
            vol = 255;
        } else {
            vol = t < 6500 ? 255 - (t - 5000) * 255 / 1500 : 0;
        }
        M5Cardputer.Speaker.setChannelVolume(CH_RUMBLE, vol);
        M5Cardputer.Speaker.setChannelVolume(CH_WHISTLE, vol * 70 / 255);
        if (t >= 6500) {
            stopAllSound();
            M5Cardputer.Speaker.setChannelVolume(CH_RUMBLE, 255);
            M5Cardputer.Speaker.setChannelVolume(CH_WHISTLE, 255);
            enter(St::Win);
        }
    }
}

void drawPause() {
    canvas.fillScreen(C_SPACE);
    drawStars(H, true);
    drawHud();
    canvas.fillRoundRect(30, 24, W - 60, 96, 6, C_PANEL);
    canvas.drawRoundRect(30, 24, W - 60, 96, 6, C_ORANGE);
    shadowText(tr(PAUSE_TITLE), W / 2, 30, C_ORANGE, 3, TC_DATUM);
    text(tr(PAUSE_TIMER) + fmtTime(pausedRemaining), W / 2, 70, C_CYAN, 1, TC_DATUM);
    canvas.fillRoundRect(40, 88, W - 80, 22, 4, C_PANEL2);
    canvas.drawRoundRect(40, 88, W - 80, 22, 4, C_YELLOW);
    if (blink()) {
        canvas.fillTriangle(48, 93, 48, 105, 56, 99, C_YELLOW);
    }
    text(tr(PAUSE_RESUME), W / 2 + 8, 93, C_TEXT, 1, TC_DATUM);
}

bool canPause() {
    return state == St::Puzzle || state == St::Solved || state == St::Computer;
}

void togglePause() {
    uint32_t now = millis();
    if (!paused) {
        pausedRemaining = remaining();
        pausedAt = now;
        paused = true;
        stopAllSound();
        morsePlaying = false;  // ESPACE pour rejouer le signal après la pause
        helpOpen = false;
    } else {
        paused = false;
        deadline = now + pausedRemaining;
        stateStart += now - pausedAt;  // l'ordinateur de bord reprend où il en était
        nextO2Beep = now + 1500;
        errFlashUntil = 0;
        penaltyPopupUntil = 0;
    }
    sfxClick();
}

// Fn seul (sans autre touche) : compte les appuis pour la pause
bool fnOnly(const KeysState &ks) {
    return ks.fn && ks.word.empty() && !ks.enter && !ks.del && !ks.tab && !ks.space;
}

void handleFn() {
    uint32_t now = millis();
    fnCount = (fnCount > 0 && now - lastFn <= FN_GAP_MS) ? fnCount + 1 : 1;
    lastFn = now;
    if (fnCount >= 3) {
        fnCount = 0;
        if (paused || canPause()) {
            togglePause();
        }
    }
}

void render(uint32_t now) {
    canvas.setFont(&fonts::efontJA_12);
    if (paused) {
        drawPause();
        canvas.pushSprite(0, 0);
        return;
    }
    switch (state) {
        case St::Lang: drawLang(); break;
        case St::Mode: drawMode(); break;
        case St::WifiList: drawWifiList(); break;
        case St::SsidEntry: drawTyping(tr(SSID_TITLE), tr(SSID_HINT), C_DIM, wifiSsid); break;
        case St::Password: drawTyping(tr(PASS_TITLE), wifiSsid, C_CYAN, wifiPass); break;
        case St::Connecting: drawConnecting(now); break;
        case St::Address: drawAddress(); break;
        case St::Title: drawTitle(now); break;
        case St::Briefing: drawBriefing(); break;
        case St::Puzzle:
            if (puzzle == 0) drawPuzzleMorse(now, true);
            else if (puzzle == 1) drawPuzzleQuiz(now);
            else if (puzzle == 2) drawPuzzlePicross();
            else drawPuzzleMorse(now, false);
            break;
        case St::Solved: drawSolved(); break;
        case St::Computer: drawComputer(now); break;
        case St::Launch: drawLaunch(now); break;
        case St::Win: drawWin(now); break;
        case St::GameOver: drawGameOver(now); break;
    }
    drawErrorFlash();
    canvas.pushSprite(0, 0);
}

}  // namespace

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, false);  // clavier démarré à la main (voir PolledKeyboardReader)
    if (M5.getBoard() == m5::board_t::board_M5CardputerADV) {
        M5Cardputer.Keyboard.begin(std::unique_ptr<KeyboardReader>(new PolledKeyboardReader()));
    } else {
        M5Cardputer.Keyboard.begin();
    }
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Speaker.begin();
    M5Cardputer.Speaker.setVolume(VOLUME);

    canvas.setColorDepth(16);
    canvas.createSprite(W, H);
    canvas.setTextWrap(false);

    randomSeed(3);  // ciel étoilé identique à chaque partie
    for (auto &s : stars) {
        s.x = random(0, W);
        s.y = random(0, H);
        s.b = random(60, 200);
    }
    buildNoise();
    buildClues();
    randomSeed(esp_random());
    prefs.begin("explorer3", false);
    bestO2 = prefs.getUInt("best_o2", 0);
    symbolsJs = buildSymbolsJs();
    mirror::setSymbols(symbolsJs.c_str());
    if (prefs.isKey("lang")) {
        lang = prefs.getUChar("lang", 0) ? 1 : 0;
        enter(St::Mode);
    } else {
        enter(St::Lang);  // premier démarrage : choix de la langue
    }
}

void loop() {
    M5Cardputer.update();
    M5Cardputer.Keyboard.updateKeyList();
    M5Cardputer.Keyboard.updateKeysState();
    uint32_t now = millis();
    runNotes(now);
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        auto ks = M5Cardputer.Keyboard.keysState();
        if (fnOnly(ks)) {
            handleFn();
        } else {
            fnCount = 0;
            if (!paused) {
                handleKey(ks);
            }
        }
    }
    now = millis();
    if (!paused) {
        update(now);
    }
    runNotes(now);
    if (mirrorMode) {
        updatePanel(now);
    }
    mirror::lockScreen();
    render(now);
    mirror::unlockScreen();
    delay(10);
}
