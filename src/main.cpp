// Escape game "Explorer 3" pour M5Stack Cardputer ADV.
// Le vaisseau s'est écrasé sur Mars : 4 énigmes en 5 minutes pour retrouver
// le code de démarrage et redécoller.
// « Cardputer seul » : code NASA, tout sur le Cardputer.
//   1. Morse lumineux (N)      -> coffre du fer à souder
//   2. QCM premier rover (A)   -> réservoirs de carburant
//   3. Picross 5x5 (S)         -> stockage des pièces détachées
//   4. Morse sonore (A)        -> ordinateur de bord (alarme O₂ coupée)
// « Multijoueur » : code ARES, deux équipes. L'équipage joue sur le Cardputer,
// le centre de contrôle sur la page web d'un PC ou d'une télé (diffusion.cpp),
// qui reçoit aussi le son. Chaque équipe a une partie des indices.
//   1. Morse lumineux (A) : l'équipage voit le voyant, le contrôle a l'alphabet
//   2. Picross (R) : l'équipage a la grille, le contrôle les chiffres
//   3. Morse sonore (E) : le contrôle l'entend, l'équipage a l'alphabet
//   4. Labyrinthe des planètes (S) : l'équipage déplace le personnage sans voir
//      les planètes, le contrôle les voit et doit connaître leur ordre ; le
//      chemin dessine un S
// Puis saisie du code (clavier codé en multijoueur), décollage et écran de fin.
// Fn appuyé 3 fois d'affilée : pause / reprise (maître du jeu).
// Record (O₂ restant) gardé en mémoire même après extinction.
// Jeu en français ou en anglais (choix au premier démarrage, textes dans textes.h).
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
constexpr uint32_t SOUND_UNIT_MS = 200;  // Morse sonore (énigme 4, 3 en multijoueur)
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

const char CODE_SOLO[] = "NASA";
const char CODE_MULTI[] = "ARES";

// Types d'énigmes, dans l'ordre de chaque mode (voir puzzleKind)
enum class Pz { Lamp, Quiz, Maze, Picross, Sound };
constexpr Pz ORDER_SOLO[4] = {Pz::Lamp, Pz::Quiz, Pz::Picross, Pz::Sound};
constexpr Pz ORDER_MULTI[4] = {Pz::Lamp, Pz::Picross, Pz::Sound, Pz::Maze};

// Labyrinthe des planètes (énigme 4 en multijoueur). 'E' entrée, 'X' sortie,
// '1' à '8' Mercure à Neptune. Un seul chemin : de l'entrée, passer par les
// 8 planètes dans l'ordre (pas haut, bas, gauche, droite), puis la sortie.
// Le chemin dessine un S (dernière lettre de ARES) dans les 3 colonnes de gauche.
// L'entrée et la sortie touchent chacune 3 planètes : ni le premier ni le
// dernier pas ne se devinent sans les noms. Aucune planète voisine d'une case
// du chemin n'est la suivante dans l'ordre : pas de fausse piste.
constexpr int MAZE_W = 4;
constexpr int MAZE_H = 5;
const char MAZE[MAZE_H][MAZE_W + 1] = {
    "21E2",
    "3534",
    "4568",
    "3776",
    "1X84",
};

// Personnage du labyrinthe (astronaute 9 x 12), aussi envoyé à la page web.
// W blanc, B visière, C reflet, G gris, D gris foncé, O orange ; '.' = transparent.
const char *const ASTRO[] = {
    "...WWW...",
    "..WWWWW..",
    ".WWBBBWW.",
    ".WBCBBBW.",
    ".WWBBBWW.",
    "..WWWWW..",
    ".GWWWWWG.",
    "GGWWOWWGG",
    ".GWWWWWG.",
    "..WW.WW..",
    "..WW.WW..",
    ".DDD.DDD.",
};
constexpr int ASTRO_W = 9;
constexpr int ASTRO_H = 12;

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

// Picross : la lettre S en solo (3e lettre de NASA), R en multijoueur (2e de ARES)
const char *const PICROSS_SOLO[5] = {
    "#####",
    "#....",
    "#####",
    "....#",
    "#####",
};
const char *const PICROSS_MULTI[5] = {
    "####.",
    "#...#",
    "####.",
    "#..#.",
    "#...#",
};

enum class St {
    Lang, Mode, WifiList, SsidEntry, Password, Connecting, Address,
    Title, Briefing, Rules, Puzzle, Solved, Computer, Launch, Win, GameOver
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
}uint32_t stateStart = 0;

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

// Morse (voyant et son)
uint32_t morseStart = 0;
bool morsePlaying = false;
uint32_t morseEnd = 0;

// Picross
bool grid[5][5];
int curX = 0;
int curY = 0;
std::vector<int> rowClues[5];
std::vector<int> colClues[5];

// Labyrinthe : position du personnage, planètes déjà passées (0 à 8), trace
int mazeR = 0;
int mazeC = 0;
int mazeStep = 0;
bool mazeTrail[MAZE_H][MAZE_W];
int mazeWrongR = -1;  // dernière mauvaise case, en rouge jusqu'à mazeWrongUntil
int mazeWrongC = -1;
uint32_t mazeWrongUntil = 0;
bool mazeDone = false;  // sortie atteinte : le S complet reste affiché MAZE_SHOW_MS
uint32_t mazeDoneAt = 0;
constexpr uint32_t MAZE_SHOW_MS = 1500;

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

// Record : plus grande réserve d'O₂ restante
Preferences prefs;
uint32_t bestO2 = 0;
bool newRecord = false;

// Multijoueur : le centre de contrôle a sa page web, le son part vers le
// navigateur au lieu du haut-parleur
bool multiMode = false;
int modeSel = 0;  // 0 Cardputer seul, 1 multijoueur, 2 langue

// Texte du mode choisi : seul ou multijoueur
const char *trm(Tx &solo, Tx &multi) {
    return multiMode ? multi[lang] : solo[lang];
}

const char *code() {
    return multiMode ? CODE_MULTI : CODE_SOLO;
}

Pz puzzleKind() {
    return (multiMode ? ORDER_MULTI : ORDER_SOLO)[puzzle];
}

// Lettre à trouver dans l'énigme en cours
char answer() {
    return code()[puzzle];
}
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

// Clavier codé de l'ordinateur de bord (multijoueur) : les touches 1 à 9
// portent des symboles, seul le centre de contrôle a la table lettre -> symbole.
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
    if (multiMode) {
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
    if (multiMode) {
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
            if (multiMode) {
                mirror::sendTone(n.at, n.freq, n.dur, n.ch);
                chanUntil[n.ch] = n.at + n.dur;
            } else {
                M5Cardputer.Speaker.tone(n.freq, n.dur, n.ch, true);
            }
        }
    }
}

bool channelPlaying(uint8_t ch) {
    if (multiMode) {
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

// « ₂ » (O₂) n'existe pas dans la police : petit 2 dessiné à la main, en indice
const char SUB2[] = "\xE2\x82\x82";
const char *const SUB2_PX[5] = {"###", "..#", "###", "#..", "###"};

void text(const String &s, int x, int y, uint16_t col, int size = 1, textdatum_t datum = TL_DATUM) {
    int k = s.indexOf(SUB2);
    if (k >= 0 && size == 1 && datum == TL_DATUM) {
        String before = s.substring(0, k);
        text(before, x, y, col);
        x += canvas.textWidth(before);
        for (int r = 0; r < 5; r++) {
            for (int c = 0; c < 3; c++) {
                if (SUB2_PX[r][c] == '#') {
                    canvas.drawPixel(x + 1 + c, y + 7 + r, col);
                }
            }
        }
        text(s.substring(k + 3), x + 5, y, col);
        return;
    }
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

    text("O₂", 3, 2, C_CYAN);
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
        text(i < found ? String(code()[i]) : String("?"), x + i * 13 + 6, 2, i < found ? C_BLACK : C_DIM, 1, TC_DATUM);
    }
}

// ---- Touches : les flèches (touches ; . , / du Cardputer, codes 1 à 4 dans
// les textes, voir textes.h) sont dessinées en triangles, et dans une indication
// « touche : action » la touche est en orange, l'action dans la couleur donnée.
constexpr int ARROW_W = 9;  // place prise par une flèche

bool isArrow(char c) {
    return c >= 1 && c <= 4;
}

void drawArrow(char c, int x, int y, uint16_t col) {
    int cx = x + 3;
    int cy = y + 6;
    switch (c) {
        case 1: canvas.fillTriangle(cx - 3, cy + 2, cx + 3, cy + 2, cx, cy - 3, col); break;   // haut
        case 2: canvas.fillTriangle(cx - 3, cy - 2, cx + 3, cy - 2, cx, cy + 3, col); break;   // bas
        case 3: canvas.fillTriangle(cx + 2, cy - 3, cx + 2, cy + 3, cx - 3, cy, col); break;   // gauche
        default: canvas.fillTriangle(cx - 2, cy - 3, cx - 2, cy + 3, cx + 3, cy, col); break;  // droite
    }
}

// Largeur des n premiers caractères de s, flèches comprises
int runWidth(const char *s, int n) {
    int w = 0;
    String run;
    for (int i = 0; i < n; i++) {
        if (isArrow(s[i])) {
            w += canvas.textWidth(run) + ARROW_W;
            run = "";
        } else {
            run += s[i];
        }
    }
    return w + canvas.textWidth(run);
}

// Dessine les n premiers caractères de s (coin haut gauche), flèches en
// arrowCol ; renvoie le x de fin
int drawRun(const char *s, int n, int x, int y, uint16_t col, uint16_t arrowCol) {
    canvas.setTextSize(1);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextColor(col);
    String run;
    for (int i = 0; i <= n; i++) {
        if (i == n || isArrow(s[i])) {
            canvas.drawString(run, x, y);
            x += canvas.textWidth(run);
            run = "";
            if (i < n) {
                drawArrow(s[i], x, y, arrowCol);
                x += ARROW_W;
            }
        } else {
            run += s[i];
        }
    }
    return x;
}

// Indication de n caractères : ce qui précède le premier « : » est la touche
void drawHintRun(const char *s, int n, int x, int y, uint16_t col, uint16_t keyCol) {
    const char *colon = static_cast<const char *>(memchr(s, ':', n));
    if (!colon) {
        drawRun(s, n, x, y, col, keyCol);
        return;
    }
    int k = colon - s;
    while (k > 0 && s[k - 1] == ' ') {
        k--;
    }
    x = drawRun(s, k, x, y, keyCol, keyCol);
    drawRun(s + k, n - k, x, y, col, keyCol);
}

void hint(const char *s, int x, int y, uint16_t col, textdatum_t datum = TL_DATUM, bool shadow = false) {
    int n = strlen(s);
    int w = runWidth(s, n);
    if (datum == TC_DATUM) {
        x -= w / 2;
    } else if (datum == TR_DATUM) {
        x -= w;
    }
    if (shadow) {
        drawHintRun(s, n, x + 1, y + 1, C_BLACK, C_BLACK);
    }
    drawHintRun(s, n, x, y, col, C_ORANGE);
}

// Pied de page : groupes « touche : action » séparés par '|', répartis sur la largeur
void drawFooter(const char *s) {
    canvas.fillRect(0, H - 14, W, 14, C_PANEL);
    canvas.drawFastHLine(0, H - 15, W, C_BORDER);
    const char *grp[4];
    int len[4];
    int n = 0;
    int total = 0;
    for (const char *p = s; n < 4;) {
        const char *e = strchr(p, '|');
        grp[n] = p;
        len[n] = e ? e - p : strlen(p);
        total += runWidth(grp[n], len[n]);
        n++;
        if (!e) {
            break;
        }
        p = e + 1;
    }
    int gap = n > 1 ? std::max(6, std::min(24, (W - 4 - total) / (n - 1))) : 0;
    int x = (W - total - gap * (n - 1)) / 2;
    for (int i = 0; i < n; i++) {
        drawHintRun(grp[i], len[i], x, H - 13, C_DIM, C_ORANGE);
        x += runWidth(grp[i], len[i]) + gap;
    }
}

void drawErrorFlash() {
    if ((int32_t)(errFlashUntil - millis()) > 0) {
        canvas.drawRect(0, 0, W, H, C_RED);
        canvas.drawRect(1, 1, W - 2, H - 2, C_RED);
        canvas.drawRect(2, 2, W - 4, H - 4, C_RED);
    }
}

void drawMorseTable(int top, int rowH);

void drawMorseHelp() {
    canvas.fillRect(0, 16, W, H - 16, C_PANEL);
    text(tr(MORSE_HELP), 4, 18, C_ORANGE);
    hint(tr(MORSE_CLOSE), W - 4, 18, C_DIM, TR_DATUM);
    drawMorseTable(34, 14);
}

// Alphabet Morse en 4 colonnes de 7 lettres
void drawMorseTable(int top, int rowH) {
    for (int i = 0; i < 26; i++) {
        int col = i / 7;
        int row = i % 7;
        int x = 2 + col * 60;
        int y = top + row * rowH;
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
    morseEnd = morseStart + morseDuration(MORSE[answer() - 'A'], LAMP_UNIT_MS);
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
    for (const char *p = MORSE[answer() - 'A']; *p; p++) {
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
    for (const char *p = MORSE[answer() - 'A']; *p; p++) {
        uint32_t on = (*p == '-' ? 3 : 1) * SOUND_UNIT_MS;
        schedule(t, MORSE_FREQ, on, CH_MORSE);
        t += on + SOUND_UNIT_MS;
    }
    morseEnd = t;
    morsePlaying = true;
}

// ---------------------------------------------------------------- picross

const char *const *picrossArt() {
    return multiMode ? PICROSS_MULTI : PICROSS_SOLO;
}

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
    const char *const *art = picrossArt();
    for (int i = 0; i < 5; i++) {
        bool row[5];
        bool col[5];
        for (int j = 0; j < 5; j++) {
            row[j] = art[i][j] == '#';
            col[j] = art[j][i] == '#';
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
    const char *const *art = picrossArt();
    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 5; c++) {
            if (grid[r][c] != (art[r][c] == '#')) {
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
    const char *names[3] = {tr(MODE_SOLO), tr(MODE_MULTI), tr(MODE_LANG)};
    const char *infos[3] = {tr(MODE_SOLO_INFO), tr(MODE_MULTI_INFO), tr(MODE_LANG_INFO)};
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
        hint(tr(WIFI_REFRESH), W - 4, 3, C_DIM, TR_DATUM);
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
        hint(tr(ADDR_CONTINUE), tx, 104, C_YELLOW);
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
        hint(tr(TITLE_START), W / 2, 118, C_YELLOW, TC_DATUM, true);
    }
}

void drawBriefing() {
    canvas.fillScreen(C_SPACE);
    drawStars(H, false);
    canvas.fillRoundRect(3, 3, W - 6, H - 22, 5, C_PANEL);
    canvas.drawRoundRect(3, 3, W - 6, H - 22, 5, C_BORDER);
    text(tr(BRIEF_TITLE), 10, 8, C_ORANGE);
    int y = wrapped(trm(BRIEF_TEXT, BRIEF_TEXT_MULTI), 10, 24, W - 20, C_TEXT);
    wrapped(tr(BRIEF_RULES), 10, y + 4, W - 20, C_CYAN);
    if (blink()) {
        hint(trm(BRIEF_START, BRIEF_NEXT), W / 2, H - 14, C_YELLOW, TC_DATUM);
    }
}

// Multijoueur : règles de l'équipage avant le chrono (le centre de contrôle a les siennes sur sa page)
void drawRules() {
    canvas.fillScreen(C_SPACE);
    drawStars(H, false);
    canvas.fillRoundRect(3, 3, W - 6, H - 22, 5, C_PANEL);
    canvas.drawRoundRect(3, 3, W - 6, H - 22, 5, C_BORDER);
    text(tr(RULES_TITLE), 10, 8, C_ORANGE);
    int y = wrapped(tr(RULES_CREW), 10, 26, W - 20, C_YELLOW);
    y = wrapped(tr(RULES_TALK), 10, y + 6, W - 20, C_TEXT);
    wrapped(tr(RULES_NO_LOOK), 10, y + 6, W - 20, C_RED);
    if (blink()) {
        hint(tr(BRIEF_START), W / 2, H - 14, C_YELLOW, TC_DATUM);
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
        wrapped(trm(P1_TEXT, P1_TEXT_MULTI), 4, 36, 140, C_TEXT, 13);
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
    if (multiMode) {
        drawFooter(tr(morsePlaying ? MORSE_PLAYING_MULTI : P1_FOOTER_MULTI));
    } else {
        drawFooter(tr(morsePlaying ? MORSE_PLAYING : light ? P1_FOOTER : P4_FOOTER));
    }
    if (helpOpen) {
        drawMorseHelp();
    }
}

// Multijoueur, énigme 3 : le centre de contrôle entend le signal, l'équipage a
// l'alphabet Morse en permanence
void drawPuzzleSoundMulti(uint32_t now) {
    canvas.fillScreen(C_SPACE);
    drawHud();
    text(tr(P3_TITLE_MULTI), 4, 18, C_ORANGE);
    drawMorseTable(33, 12);
    if (morsePlaying && (int32_t)(now - morseEnd) >= 0) {
        morsePlaying = false;
    }
    if (wrongShown(now)) {
        drawFooter("");
        text(String(wrongLetter) + tr(DENIED), W / 2, H - 13, C_RED, 1, TC_DATUM);
    } else {
        drawFooter(tr(morsePlaying ? MORSE_PLAYING_MULTI : P3_FOOTER_MULTI));
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

uint16_t astroColor(char c) {
    switch (c) {
        case 'W': return C_WHITE;
        case 'B': return C_BLUE;
        case 'C': return C_CYAN;
        case 'G': return C_GREY;
        case 'D': return C_DGREY;
        case 'O': return C_ORANGE;
        default: return 0;
    }
}

void drawAstro(int x, int y) {
    for (int r = 0; r < ASTRO_H; r++) {
        for (int c = 0; c < ASTRO_W; c++) {
            if (ASTRO[r][c] != '.') {
                canvas.drawPixel(x + c, y + r, astroColor(ASTRO[r][c]));
            }
        }
    }
}

bool mazeWrongShown(uint32_t now) {
    return (int32_t)(mazeWrongUntil - now) > 0;
}

// Multijoueur, énigme 4 : l'équipage voit la grille vide, le personnage, sa
// trace et la sortie ; les planètes ne sont que sur la page du centre de contrôle
void drawPuzzleMaze(uint32_t now) {
    canvas.fillScreen(C_SPACE);
    drawHud();
    text(tr(P4_TITLE), 4, 19, C_ORANGE);
    const int cell = 17;
    const int gx = 4;
    const int gy = 34;
    bool wrong = mazeWrongShown(now);
    for (int r = 0; r < MAZE_H; r++) {
        for (int c = 0; c < MAZE_W; c++) {
            int x = gx + c * cell;
            int y = gy + r * cell;
            uint16_t bg = mazeTrail[r][c] ? rgb(140, 70, 15) : rgb(30, 34, 52);
            if (wrong && r == mazeWrongR && c == mazeWrongC) {
                bg = rgb(140, 25, 25);
            }
            canvas.fillRect(x, y, cell, cell, bg);
            canvas.drawRect(x, y, cell + 1, cell + 1, C_BORDER);
            if (MAZE[r][c] == 'X' && !mazeTrail[r][c]) {  // sortie : trappe verte et flèche
                canvas.fillRoundRect(x + 2, y + 2, cell - 3, cell - 3, 2, rgb(20, 90, 40));
                canvas.drawRoundRect(x + 2, y + 2, cell - 3, cell - 3, 2, C_GREEN);
                canvas.fillTriangle(x + 6, y + 4, x + 6, y + 13, x + 11, y + 8, C_GREEN);
            }
        }
    }
    drawAstro(gx + mazeC * cell + 4, gy + mazeR * cell + 3);
    const int px = gx + MAZE_W * cell + 8;
    wrapped(tr(P4_TEXT_MAZE), px, 36, W - px - 4, C_TEXT, 13);
    if (wrong) {
        wrapped(tr(P4_WRONG_MAZE), px, 92, W - px - 4, C_RED, 13);
    }
    drawFooter(tr(P4_FOOTER_MAZE));
}

// Personnage sur l'entrée, trace effacée
void mazeReset() {
    for (int r = 0; r < MAZE_H; r++) {
        for (int c = 0; c < MAZE_W; c++) {
            mazeTrail[r][c] = false;
            if (MAZE[r][c] == 'E') {
                mazeR = r;
                mazeC = c;
            }
        }
    }
    mazeStep = 0;
    mazeDone = false;
}

void drawPuzzlePicross() {
    canvas.fillScreen(C_SPACE);
    drawHud();
    const int cell = 17;
    const int gx = 32;
    const int gy = 47;
    canvas.setFont(&fonts::Font0);
    for (int i = 0; i < 5; i++) {
        // Indices des colonnes (empilés au-dessus). En multijoueur, l'équipage
        // n'a que des « ? » : les chiffres sont sur la page du centre de contrôle.
        uint16_t cc = colOk(i) ? C_GREEN : C_TEXT;
        int n = colClues[i].size();
        for (int k = 0; k < n; k++) {
            String clue = multiMode ? String("?") : String(colClues[i][k]);
            text(clue, gx + i * cell + cell / 2, gy - 3 - (n - k) * 9, cc, 1, TC_DATUM);
        }
        // Indices des lignes (à gauche)
        uint16_t rc = rowOk(i) ? C_GREEN : C_TEXT;
        String s;
        for (size_t k = 0; k < rowClues[i].size(); k++) {
            s += (k ? " " : "") + (multiMode ? String("?") : String(rowClues[i][k]));
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

    const int px = 128;
    text(trm(P3_TITLE, P2_TITLE_MULTI), px, 19, C_ORANGE);
    text(trm(P3_PLACE, P2_PLACE_MULTI), px, 33, C_TEXT);
    wrapped(trm(P3_TEXT, P2_TEXT_MULTI), px, 50, W - px - 4, C_DIM, 13);
    hint(tr(P3_MOVE), px, 104, C_CYAN);
    hint(tr(P3_LIGHT), px, 118, C_CYAN);
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
    text(String(answer()), 214 + 1, 82, C_BLACK, 2, TC_DATUM);
    if (blink()) {
        drawFooter(tr(SOLVED_NEXT));
    } else {
        drawFooter("");
    }
}

constexpr int WIN_INFO_SPEED = 45;  // défilement du bandeau de fin, en pixels par seconde
constexpr int WIN_BAND_Y = 103;     // bandeau : lignes 103 à 117 de l'écran
constexpr int WIN_BAND_H = 15;

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
    // Les lettres du code (A, R, E, S) + d'autres lettres, réparties au hasard sur les touches
    char letters[9];
    int n = 0;
    for (const char *c = code(); *c; c++) {
        if (std::find(letters, letters + n, *c) == letters + n) {
            letters[n++] = *c;
        }
    }
    while (n < 9) {
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
    js += "];const ASTRO=[";
    for (int r = 0; r < ASTRO_H; r++) {
        js += r ? ",'" : "'";
        js += ASTRO[r];
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

// Indices du picross pour la page du centre de contrôle : lignes séparées
// par « / », chiffres d'une ligne par « . » (ex. « 3.1/1.1.1 »)
String cluesText(const std::vector<int> *clues) {
    String s;
    for (int i = 0; i < 5; i++) {
        s += i ? "/" : "";
        for (size_t k = 0; k < clues[i].size(); k++) {
            s += (k ? "." : "") + String(clues[i][k]);
        }
    }
    return s;
}

// Page du centre de contrôle (multijoueur), voir mirror::setPanel :
//   0 copie de l'écran du Cardputer (titre, journal de bord, parties réparées, pause, fins)
//   R règles du centre de contrôle
//   1 à 4 énigme en cours, C table du clavier codé : suivis du chrono et du
//   nombre d'erreurs (la page clignote en rouge à chaque erreur), puis des
//   données de la page. Renvoyée à chaque changement et toutes les 500 ms (chrono).
void updatePanel(uint32_t now) {
    static String last;
    static uint32_t lastSent = 0;
    String page = "0";
    String data;
    if (!paused && state == St::Rules) {
        page = "R";
    } else if (!paused && state == St::Puzzle) {
        page = String(puzzle + 1);
        if (puzzleKind() == Pz::Maze) {
            // grille,position,trace,mauvaise case (-1 : aucune)
            String trail;
            for (int r = 0; r < MAZE_H; r++) {
                data += MAZE[r];
                for (int c = 0; c < MAZE_W; c++) {
                    trail += mazeTrail[r][c] ? '1' : '0';
                }
            }
            int bad = mazeWrongShown(now) ? mazeWrongR * MAZE_W + mazeWrongC : -1;
            data += "," + String(mazeR * MAZE_W + mazeC) + "," + trail + "," + String(bad);
        } else if (puzzleKind() == Pz::Picross) {
            for (int r = 0; r < 5; r++) {
                for (int c = 0; c < 5; c++) {
                    data += grid[r][c] ? '1' : '0';
                }
            }
            data += "," + cluesText(rowClues) + "," + cluesText(colClues);
        }
    } else if (!paused && state == St::Computer) {
        page = "C";
        String table;
        for (char c = 'A'; c <= 'Z'; c++) {
            for (int k = 0; k < 9; k++) {
                if (keyLetter[k] == c) {
                    table += c;
                    table += "0123456789ab"[keySym[k]];
                }
            }
        }
        data = String(typedCode.length()) + "," + table;
    }
    String key = page;
    if (page != "0" && page != "R") {
        key += "," + String(errCount) + "," + data;
    }
    bool timed = page != "0" && page != "R";
    if (key == last && (!timed || now - lastSent < 500)) {
        return;
    }
    last = key;
    lastSent = now;
    if (timed) {
        key = page + "," + String(remaining()) + "," + String(errCount) + "," + data;
    }
    mirror::setPanel(key);
}

// Bandeau de l'écran de fin : la page le fait défiler elle-même, plus fluide
// qu'une copie de l'écran qui change à chaque image
void updateBanner() {
    static bool shown = false;
    bool want = state == St::Win;
    if (want != shown) {
        shown = want;
        mirror::setBanner(want ? String(tr(WIN_INFO_MULTI)) : String(), WIN_BAND_Y, WIN_BAND_H);
    }
}

void drawComputer(uint32_t now) {
    canvas.fillScreen(C_BLACK);
    drawHud();
    if (multiMode && keypadShown(now)) {
        drawKeypad(now);
        return;
    }
    int n = termVisible(now);
    for (int i = 0; i < n; i++) {
        uint16_t col = (i == TERM_COUNT - 1) ? C_YELLOW : C_TERM;
        String line = tr(TERM_LINES[i]);
        if (i == TERM_COUNT - 1) {
            line += code();
        }
        text(line, 4, 19 + i * 13, col);
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
    // Bandeau qui défile : la signification du code. En multijoueur, la page le
    // fait défiler elle-même (mirror::setBanner, voir updatePanel)
    canvas.fillRect(0, WIN_BAND_Y, W, WIN_BAND_H, C_PANEL);
    canvas.drawFastHLine(0, WIN_BAND_Y, W, C_BORDER);
    canvas.drawFastHLine(0, WIN_BAND_Y + WIN_BAND_H - 1, W, C_BORDER);
    String info = trm(WIN_INFO_SOLO, WIN_INFO_MULTI);
    int len = canvas.textWidth(info) + W / 2;
    int x = W - (int)((uint64_t)(now - stateStart) * WIN_INFO_SPEED / 1000 % (W + len));
    text(info, x, WIN_BAND_Y + 2, C_TEXT);
    if (blink()) {
        hint(tr(WIN_AGAIN), W - 4, 121, C_YELLOW, TR_DATUM, true);
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
        hint(tr(LOST_AGAIN), W / 2, 118, C_YELLOW, TC_DATUM, true);
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
    Pz k = puzzleKind();
    if (k == Pz::Lamp) {
        startMorseLamp();
        morseStart += 800;
        morseEnd += 800;
    } else if (k == Pz::Picross) {
        buildClues();  // dessin différent selon le mode
        memset(grid, 0, sizeof(grid));
        curX = curY = 0;
    } else if (k == Pz::Maze) {
        mazeReset();
        mazeWrongUntil = 0;
    } else if (k == Pz::Sound) {
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

void solvePuzzle(bool sound = true) {
    if (sound) {
        sfxSuccess();
    }
    wrongLetter = 0;
    helpOpen = false;
    cancelChannel(CH_MORSE);
    morsePlaying = false;
    if (puzzle == 3) {
        typedCode = "";
        termShown = 0;
        if (multiMode) {
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
    if (multiMode) {
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

// Un pas dans le labyrinthe : la case doit être la planète suivante (puis la
// sortie après Neptune). Sinon -10 s, retour à l'entrée et trace effacée.
// Une flèche vers le bord de la grille ne fait rien.
void mazeMove(int dr, int dc) {
    int r = mazeR + dr;
    int c = mazeC + dc;
    if (r < 0 || r >= MAZE_H || c < 0 || c >= MAZE_W) {
        return;
    }
    char next = mazeStep < 8 ? '1' + mazeStep : 'X';
    if (MAZE[r][c] != next) {
        mazeWrongR = r;
        mazeWrongC = c;
        mazeWrongUntil = millis() + 1500;
        mazeReset();
        penalty();
        return;
    }
    mazeR = r;
    mazeC = c;
    if (next == 'X') {
        for (int rr = 0; rr < MAZE_H; rr++) {
            for (int cc = 0; cc < MAZE_W; cc++) {
                if (MAZE[rr][cc] == 'E' || MAZE[rr][cc] == 'X') {
                    mazeTrail[rr][cc] = true;
                }
            }
        }
        mazeDone = true;
        mazeDoneAt = millis();
        sfxSuccess();
        return;
    }
    mazeTrail[r][c] = true;
    mazeStep++;
    sfxClick();
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
                multiMode = modeSel == 1;
                if (!multiMode) {
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
                multiMode = false;
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
                if (multiMode) {
                    enter(St::Rules);  // le chrono démarre après les règles
                } else {
                    startGame();
                }
            }
            break;

        case St::Rules:
            if (ks.enter) {
                sfxClick();
                startGame();
            }
            break;

        case St::Puzzle: {
            Pz k = puzzleKind();
            bool morse = k == Pz::Lamp || k == Pz::Sound;
            // Alphabet Morse sur TAB : seulement sur le Cardputer seul (en
            // multijoueur, le centre de contrôle l'a pour le voyant, et l'équipage
            // l'a en permanence pour le son)
            bool help = morse && !multiMode;
            if (help && helpOpen) {
                helpOpen = false;  // n'importe quelle touche ferme l'aide
                break;
            }
            if (help && ks.tab) {
                helpOpen = true;
                break;
            }
            if (morse && ks.space) {
                if (!morsePlaying) {
                    if (k == Pz::Lamp) {
                        startMorseLamp();
                    } else {
                        startMorseSound();
                    }
                }
                break;
            }
            char l = letterOf(ks);
            if (morse && l) {
                answerLetter(l, answer());
            } else if (k == Pz::Quiz && l >= 'A' && l <= 'D') {
                answerLetter(l, answer());
            } else if (k == Pz::Maze && !mazeDone) {
                if (hasChar(ks, ';')) mazeMove(-1, 0);
                else if (hasChar(ks, '.')) mazeMove(1, 0);
                else if (hasChar(ks, ',')) mazeMove(0, -1);
                else if (hasChar(ks, '/')) mazeMove(0, 1);
            } else if (k == Pz::Picross) {
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
            if (multiMode) {
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
                    if (keypadCode() == code()) {
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
                if (typedCode == code()) {
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
        // Alarme oxygène, de plus en plus rapide (muette pendant les énigmes de Morse)
        bool mute = state == St::Puzzle && (puzzleKind() == Pz::Lamp || puzzleKind() == Pz::Sound);
        if (!mute && (int32_t)(now - nextO2Beep) >= 0) {
            o2Beeps(now);
            uint32_t rem = remaining();
            nextO2Beep = now + 1500 + (uint32_t)((uint64_t)8500 * rem / GAME_MS);
        } else if (mute) {
            nextO2Beep = now + 1500;
        }
    }

    if (state == St::Puzzle && puzzleKind() == Pz::Maze && mazeDone && now - mazeDoneAt >= MAZE_SHOW_MS) {
        solvePuzzle(false);  // le son de réussite est parti à l'arrivée sur la sortie
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
    hint(tr(PAUSE_RESUME), W / 2 + 8, 93, C_TEXT, TC_DATUM);
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
        mazeDoneAt += now - pausedAt;
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
        case St::Rules: drawRules(); break;
        case St::Puzzle:
            switch (puzzleKind()) {
                case Pz::Lamp: drawPuzzleMorse(now, true); break;
                case Pz::Quiz: drawPuzzleQuiz(now); break;
                case Pz::Maze: drawPuzzleMaze(now); break;
                case Pz::Picross: drawPuzzlePicross(); break;
                case Pz::Sound:
                    if (multiMode) {
                        drawPuzzleSoundMulti(now);
                    } else {
                        drawPuzzleMorse(now, false);
                    }
                    break;
            }
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
    if (multiMode) {
        updatePanel(now);
        updateBanner();
    }
    mirror::lockScreen();
    render(now);
    mirror::unlockScreen();
    delay(10);
}
