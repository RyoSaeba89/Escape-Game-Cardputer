// Simulateur PC : lance le jeu (src/main.cpp, sans changement) dans une fenêtre
// SDL avec le vrai M5GFX. Clavier du PC = clavier du Cardputer (voir sim/LISEZMOI.md).
// Options : --lang fr|en (saute l'écran de langue)
//           --script "1000 enter; 2500 shot titre.bmp; 2600 quit" (actions horodatées)
#include <M5Cardputer.h>
#include <Preferences.h>

#include <chrono>
#include <deque>
#include <mutex>
#include <random>
#include <sstream>
#include <thread>

void setup();
void loop();

M5Cardputer_Class M5Cardputer;
M5_Class M5;

std::map<std::string, std::string> &Preferences::store() {
    static std::map<std::string, std::string> v;
    return v;
}

// ---------------------------------------------------------------- Arduino

namespace {
const auto t0 = std::chrono::steady_clock::now();
uint32_t randNext = 1;  // même générateur que newlib (rand() de l'ESP32)
bool useHwRandom = true;
}  // namespace

uint32_t millis() {
    return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
}

void delay(uint32_t ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

uint32_t esp_random() {
    static std::random_device rd;
    return rd();
}

void randomSeed(unsigned long seed) {
    if (seed != 0) {
        randNext = seed;
        useHwRandom = false;
    }
}

namespace {
uint64_t newlibState = 1;
int newlibRand() {
    static uint32_t seeded = 0;
    if (seeded != randNext) {  // srand() appelé par randomSeed()
        newlibState = randNext;
        seeded = randNext;
    }
    newlibState = newlibState * 6364136223846793005ULL + 1;
    return (int)((newlibState >> 32) & 0x7fffffff);
}
}  // namespace

long random(long howbig) {
    if (howbig <= 0) {
        return 0;
    }
    uint32_t v = useHwRandom ? esp_random() : (uint32_t)newlibRand();
    return v % howbig;
}

long random(long howsmall, long howbig) {
    if (howsmall >= howbig) {
        return howsmall;
    }
    return random(howbig - howsmall) + howsmall;
}

// ---------------------------------------------------------------- clavier

namespace {

std::mutex keyLock;
std::deque<KeysState> pending;
bool shotRequested = false;

struct Step {
    uint32_t at;
    std::string action;
    std::string arg;
};
std::vector<Step> script;
size_t scriptPos = 0;

KeysState keyChar(char c) {
    KeysState k;
    k.word.push_back(c);
    return k;
}

void push(const KeysState &k) {
    std::lock_guard<std::mutex> g(keyLock);
    pending.push_back(k);
}

// Nom de touche du script -> appui
bool namedKey(const std::string &n, KeysState &k) {
    k = KeysState();
    if (n == "enter") k.enter = true;
    else if (n == "del") k.del = true;
    else if (n == "tab") k.tab = true;
    else if (n == "fn") k.fn = true;
    else if (n == "space") { k.space = true; k.word.push_back(' '); }
    else if (n == "esc") k.word.push_back('`');
    else if (n == "up") k.word.push_back(';');
    else if (n == "down") k.word.push_back('.');
    else if (n == "left") k.word.push_back(',');
    else if (n == "right") k.word.push_back('/');
    else if (n.size() == 1) k.word.push_back(n[0]);
    else return false;
    return true;
}

// Capture de l'écran en BMP 24 bits (240×135)
void saveShot(const std::string &path) {
    const int w = 240, h = 135;
    std::vector<uint8_t> rgb(w * h * 3);
    M5Cardputer.Display.readRectRGB(0, 0, w, h, rgb.data());
    FILE *f = fopen(path.c_str(), "wb");
    if (!f) {
        return;
    }
    uint32_t size = 54 + w * h * 3;
    uint8_t hd[54] = {'B', 'M'};
    memcpy(hd + 2, &size, 4);
    hd[10] = 54;
    hd[14] = 40;
    memcpy(hd + 18, &w, 4);
    memcpy(hd + 22, &h, 4);
    hd[26] = 1;
    hd[28] = 24;
    fwrite(hd, 1, 54, f);
    for (int y = h - 1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            const uint8_t *p = &rgb[(y * w + x) * 3];
            uint8_t bgr[3] = {p[2], p[1], p[0]};
            fwrite(bgr, 1, 3, f);
        }
    }
    fclose(f);
    printf("capture : %s\n", path.c_str());
    fflush(stdout);
}

void runScript() {
    uint32_t now = millis();
    while (scriptPos < script.size() && script[scriptPos].at <= now) {
        const Step &s = script[scriptPos++];
        KeysState k;
        if (s.action == "shot") {
            saveShot(s.arg);
        } else if (s.action == "quit") {
            std::exit(0);
        } else if (s.action == "type") {
            for (char c : s.arg) push(keyChar(c));
        } else if (namedKey(s.action, k)) {
            push(k);
        }
    }
}

int SDLCALL onEvent(void *, SDL_Event *e) {
    if (e->type == SDL_TEXTINPUT) {
        for (const char *p = e->text.text; *p; p++) {
            if (*p > ' ' && *p < 127) {
                push(keyChar(*p));
            }
        }
    } else if (e->type == SDL_KEYDOWN) {
        KeysState k;
        const char *n = nullptr;
        switch (e->key.keysym.sym) {
            case SDLK_RETURN: case SDLK_KP_ENTER: n = "enter"; break;
            case SDLK_BACKSPACE: case SDLK_DELETE: n = "del"; break;
            case SDLK_TAB: n = "tab"; break;
            case SDLK_SPACE: n = "space"; break;
            case SDLK_ESCAPE: n = "esc"; break;
            case SDLK_F1: n = "fn"; break;
            case SDLK_UP: n = "up"; break;
            case SDLK_DOWN: n = "down"; break;
            case SDLK_LEFT: n = "left"; break;
            case SDLK_RIGHT: n = "right"; break;
            case SDLK_F12: {
                std::lock_guard<std::mutex> g(keyLock);
                shotRequested = true;
                break;
            }
            default: break;
        }
        if (n && namedKey(n, k)) {
            push(k);
        }
    }
    return 1;
}

}  // namespace

// Un appui par tour de boucle, suivi d'un tour « relâché », comme le vrai clavier
void Keyboard_Class::updateKeysState() {
    runScript();
    bool shot = false;
    KeysState next;
    bool have = false;
    {
        std::lock_guard<std::mutex> g(keyLock);
        shot = shotRequested;
        shotRequested = false;
        if (!_release && !pending.empty()) {
            next = pending.front();
            pending.pop_front();
            have = true;
        }
    }
    if (shot) {
        static int n = 0;
        saveShot("capture-" + std::to_string(++n) + ".bmp");
    }
    if (_release) {
        _release = false;
        _change = true;
        _pressed = false;
        _state = KeysState();
    } else if (have) {
        _state = next;
        _change = _pressed = true;
        _release = true;
    } else {
        _change = _pressed = false;
    }
}

// ---------------------------------------------------------------- lancement

namespace {
int gameThread(bool *running) {
    SDL_AddEventWatch(onEvent, nullptr);  // SDL est initialisé à ce moment
    setup();
    while (*running) {
        loop();
    }
    return 0;
}
}  // namespace

int main(int argc, char **argv) {
    for (int i = 1; i + 1 < argc; i += 2) {
        std::string opt = argv[i];
        if (opt == "--lang") {
            Preferences::store()["lang"] = std::string(argv[i + 1]) == "en" ? "1" : "0";
        } else if (opt == "--script") {
            std::stringstream ss(argv[i + 1]);
            std::string item;
            while (std::getline(ss, item, ';')) {
                std::stringstream is(item);
                Step s;
                if (is >> s.at >> s.action) {
                    std::getline(is >> std::ws, s.arg);
                    script.push_back(s);
                }
            }
        }
    }
    SDL_SetMainReady();
    // Raccourcis de M5GFX (zoom 1 à 6, rotation R/L) sur Ctrl droit seulement :
    // les touches seules vont au jeu
    lgfx::Panel_sdl::setShortcutKeymod(KMOD_RCTRL);
    return lgfx::Panel_sdl::main(gameThread);
}
