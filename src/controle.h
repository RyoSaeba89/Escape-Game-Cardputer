// Centre de contrôle sur le Cardputer : il suit la partie d'un équipage
// (Cardputer, Android ou Pi) sur le réseau, comme la page web du jeu.
//
// Inclus par main.cpp dans son espace de noms (fonctions de dessin, son,
// mémoire et textes du jeu). Le protocole est le cœur commun avec
// Explorer3-Android et Explorer3-RaspberryPi (src/controle/*.c) ; ici, les
// écrans à 240×135 (maquettes validées par l'utilisateur), le son par le
// haut-parleur et le bandeau de fin. Android et le Pi remplacent ce fichier par
// le leur : « Centre de contrôle » y ouvre leur propre écran.
//
// Ce que main.cpp appelle (Android et le Pi fournissent les mêmes fonctions) :
//   start()        le réseau est là (box ou réseau Explorer3)
//   key(ks)        une touche ; false : quitter
//   update(now)    à chaque tour ; false : quitter
//   draw(now)      dessine dans canvas
//   stop()         on quitte : retour au choix du rôle
//
// Écrans : recherche de l'équipage (TAB : taper son adresse), liste s'il y en a
// plusieurs, saisie de l'adresse, connexion, erreur, puis la partie : copie de
// l'écran de l'équipage, ou la page de l'énigme en cours (TAB : consignes).
// ESC revient à la recherche, puis au menu des rôles.

namespace controle {

enum class Cs { Search, None, List, Address, Probe, Connecting, Failed, Stream };
enum Reason { R_UNREACHABLE, R_REFUSED, R_NOT_GAME, R_NO_SOCKET, R_LOST, R_NO_MEMORY };

constexpr uint32_t RETRY_MS = 1000;   // reconnexion pendant la partie
constexpr uint32_t WAIT_MS = 30000;   // connexion sans réponse
constexpr uint32_t HINT_MS = 5000;    // « ESC : changer d'équipage » au début
constexpr int LIST_MAX = 4;           // équipages montrés dans la liste
constexpr int READ_MAX = 32 * 1024;   // lu par tour de boucle

session_t *ses = nullptr;  // grand (copie de l'écran) : alloué à la première entrée
Cs cs = Cs::Search;
int reason = R_UNREACHABLE;
uint8_t self[4];
String netName;
found_t found[FINDER_MAX];
int nfound = 0;
int listSel = 0;
uint8_t target[4];
int targetPort = 0;
String typed;  // adresse tapée
uint32_t since = 0;
uint32_t streamSince = 0;
uint32_t retryAt = 0;
bool linked = false;
bool connecting = false;
bool helpOpen = false;
char helpPage = 0;
int helpScroll = 0;  // consignes plus hautes que l'écran : ▲▼ les font défiler
int helpMax = 0;
uint8_t rxbuf[1460];

// Bandeau de l'écran de fin (message B), défilé ici
char band[BANNER_MAX + 1];
int bandY = 0, bandH = 0;
uint32_t bandStart = 0;

// Son : les notes passent par schedule() du jeu ; arrêts et grondement à leur heure
struct Pending {
    uint32_t at;
    uint8_t ch;  // 255 : tous
    bool rumble;
    bool used;
};
Pending pending[8];
bool rumbleOn = false;
uint32_t rumbleAt = 0;

// Langue des pages : celle de l'équipage dès qu'il l'a donnée
int pageLang() {
    return ses && ses->lang_known ? ses->lang : lang;
}

const char *L(Tx &t) {
    return t[pageLang()];
}

// « ÉNIGME 2/4 : RÉSERVOIRS » -> « ÉNIGME 2/4 » et « RÉSERVOIRS »
void splitTitle(const char *t, String &head, String &tail) {
    const char *colon = strchr(t, ':');
    if (!colon) {
        head = t;
        tail = "";
        return;
    }
    const char *end = colon;
    while (end > t && end[-1] == ' ') {
        end--;
    }
    const char *next = colon + 1;
    while (*next == ' ') {
        next++;
    }
    head = String(t).substring(0, end - t);
    tail = next;
}

// ---------------------------------------------------------------- son et bandeau

void stopRumble() {
    rumbleOn = false;
    M5Cardputer.Speaker.stop(CH_RUMBLE);
    M5Cardputer.Speaker.setChannelVolume(CH_RUMBLE, 255);
    M5Cardputer.Speaker.setChannelVolume(CH_WHISTLE, 70);
}

void addPending(uint32_t at, uint8_t ch, bool rumble) {
    for (auto &p : pending) {
        if (!p.used) {
            p = {at, ch, rumble, true};
            return;
        }
    }
}

// Messages B, T, S, R du cœur commun (session.c)
void media(void *, const msg_t *m, const clocksync_t *clk, uint32_t now) {
    if (m->kind == 'B') {
        size_t n = std::min((size_t)m->text_len, sizeof(band) - 1);
        memcpy(band, m->text, n);
        band[n] = 0;
        bandY = m->band_y;
        bandH = m->band_h;
        bandStart = now;
        return;
    }
    uint32_t at = clocksync_to_local(clk, m->at) + SOUND_LAT_MS;
    if (m->kind == 'T') {
        schedule(at, m->freq, m->dur, m->ch);
    } else if (m->kind == 'S') {
        addPending(at, m->ch, false);
    } else if (m->kind == 'R') {
        addPending(at, 0, true);
    }
}

void runSound(uint32_t now) {
    for (auto &p : pending) {
        if (!p.used || (int32_t)(now - p.at) < 0) {
            continue;
        }
        p.used = false;
        if (p.rumble) {
            M5Cardputer.Speaker.setChannelVolume(CH_RUMBLE, 0);
            M5Cardputer.Speaker.playRaw(noiseBuf, NOISE_LEN, 8000, false, 8, CH_RUMBLE, true);
            rumbleOn = true;
            rumbleAt = p.at;
        } else if (p.ch == 255) {
            stopAllSound();
            stopRumble();
        } else {
            cancelChannel(p.ch);
            if (p.ch == CH_RUMBLE) {
                stopRumble();
            }
        }
    }
    // Montée et descente du grondement et du sifflement, comme au décollage
    if (rumbleOn) {
        uint32_t t = now - rumbleAt;
        int vol = t < 1500 ? t * 255 / 1500 : t < 5000 ? 255 : t < 6500 ? 255 - (t - 5000) * 255 / 1500 : 0;
        M5Cardputer.Speaker.setChannelVolume(CH_RUMBLE, vol);
        M5Cardputer.Speaker.setChannelVolume(CH_WHISTLE, vol * 70 / 255);
        if (t >= 6500) {
            stopRumble();
        }
    }
}

// ---------------------------------------------------------------- réseau

void nonce16(uint8_t out[16]) {
    for (int i = 0; i < 16; i++) {
        out[i] = (uint8_t)random(0, 256);
    }
}

bool beginSession(uint32_t now) {
    char req[256];
    char host[16];
    uint8_t nonce[16];
    addr_format(target, host);
    nonce16(nonce);
    band[0] = 0;  // le bandeau repart de rien, comme la page
    int n = session_begin(ses, now, host, (unsigned)targetPort, nonce, req, sizeof(req));
    if (n < 0 || link_write(req, n) < 0) {
        link_close();
        return false;
    }
    return true;
}

int reasonConnect(int err) {
    if (err == -ECONNRESET || err == -ECONNREFUSED) {
        return R_REFUSED;  // l'appareil répond, mais rien n'écoute
    }
    if (err == -ENOMEM || err == -ENOBUFS || err == -ENFILE || err == -EMFILE) {
        return R_NO_SOCKET;
    }
    return R_UNREACHABLE;
}

bool notGame(int err) {
    return err == WS_ERR_HS_TOOLONG || err == WS_ERR_HS_STATUS || err == WS_ERR_HS_UPGRADE ||
           err == WS_ERR_HS_ACCEPT || err == SES_ERR_HS_LATE;
}

void leaveStream() {
    if (linked) {
        uint8_t bye[16];
        int n = session_close_frame(ses, bye, sizeof(bye));
        if (n > 0) {
            link_write(bye, n);
        }
    }
    link_close();
    linked = connecting = false;
    ses->state = SES_IDLE;
    helpOpen = false;
    band[0] = 0;
    stopAllSound();
    stopRumble();
}

void startSearch(uint32_t now) {
    finder_start(self, nullptr);
    since = now;
    cs = Cs::Search;
}

void connectTo(const uint8_t ip[4], int port, uint32_t now) {
    memcpy(target, ip, 4);
    targetPort = port;
    since = now;
    int r = link_start(ip, port);
    if (r < 0) {
        reason = reasonConnect(r);
        cs = Cs::Failed;
        return;
    }
    cs = Cs::Connecting;
}

// Saisie : l'adresse mémorisée si elle est sur ce réseau, sinon les 3 premiers nombres de celle-ci
void toAddress(uint32_t now) {
    uint8_t saved[4];
    String s = prefs.getString("ctl_ip", "");
    if (ip_parse(s.c_str(), s.length(), saved) && addr_check(saved, self) == ADDR_OK) {
        typed = s;
    } else {
        char ip[16];
        addr_format(self, ip);
        *(strrchr(ip, '.') + 1) = 0;  // « 192.168.1. »
        typed = ip;
    }
    since = now;
    cs = Cs::Address;
}

// État de l'adresse tapée : -1 incomplète, sinon ADDR_...
int typedStatus(uint8_t ip[4]) {
    if (!ip_parse(typed.c_str(), typed.length(), ip)) {
        return -1;
    }
    return addr_check(ip, self);
}

// ---------------------------------------------------------------- appelé par main.cpp

// Le réseau est là (box ou réseau Explorer3)
void start() {
    uint32_t now = millis();
    mirror::stopServer();  // les serveurs d'une partie en équipage : arrêtés
    String ip = mirror::ipAddress();
    if (!ip_parse(ip.c_str(), ip.length(), self)) {
        memset(self, 0, 4);
    }
    netName = mirror::accessPoint() ? String(mirror::AP_SSID) : wifiSsid;
    memset(pending, 0, sizeof(pending));
    M5Cardputer.Speaker.setChannelVolume(CH_WHISTLE, 70);  // le sifflement, comme sur la page
    linked = connecting = helpOpen = false;
    if (!ses) {
        ses = static_cast<session_t *>(malloc(sizeof(session_t)));
        if (!ses) {
            reason = R_NO_MEMORY;
            cs = Cs::Failed;
            return;
        }
        session_init(ses, media, nullptr);
    }
    startSearch(now);
}

// Retour au menu des rôles
void stop() {
    if (ses && (linked || connecting)) {
        leaveStream();
    }
    link_close();
    stopAllSound();
    stopRumble();
    M5Cardputer.Speaker.setChannelVolume(CH_WHISTLE, 255);
}

// Une touche ; false : quitter le centre de contrôle
bool key(const KeysState &ks) {
    uint32_t now = millis();
    bool esc = hasChar(ks, '`') && !ks.shift;
    switch (cs) {
        case Cs::Search:
            if (ks.tab) {
                toAddress(now);
            } else if (esc) {
                return false;
            }
            break;
        case Cs::None:
            if (ks.enter) {
                startSearch(now);
            } else if (ks.tab) {
                toAddress(now);
            } else if (esc) {
                return false;
            }
            break;
        case Cs::List: {
            int n = std::min(nfound, LIST_MAX);
            if (hasChar(ks, ';') && listSel > 0) {
                listSel--;
                sfxClick();
            } else if (hasChar(ks, '.') && listSel + 1 < n) {
                listSel++;
                sfxClick();
            } else if (ks.enter) {
                connectTo(found[listSel].ip, found[listSel].port, now);
            } else if (ks.tab) {
                toAddress(now);
            } else if (esc) {
                return false;
            }
            break;
        }
        case Cs::Address: {
            uint8_t ip[4];
            if (ks.enter) {
                int st = typedStatus(ip);
                if (st == ADDR_OK || st == ADDR_OTHER_NET) {  // autre réseau : prévenu, pas interdit
                    memcpy(target, ip, 4);
                    targetPort = 0;
                    finder_start(self, ip);  // quel port : Cardputer et Pi, ou Android
                    since = now;
                    cs = Cs::Probe;
                }
            } else if (esc) {
                startSearch(now);
            } else if (ks.del) {
                if (!typed.isEmpty()) {
                    typed.remove(typed.length() - 1);
                }
            } else {
                for (char c : ks.word) {
                    if (((c >= '0' && c <= '9') || c == '.') && typed.length() < 15) {
                        typed += c;
                    }
                }
            }
            break;
        }
        case Cs::Probe:
            if (esc) {
                cs = Cs::Address;
            }
            break;
        case Cs::Connecting:
            if (esc) {
                link_close();
                startSearch(now);
            }
            break;
        case Cs::Failed:
            if (ks.enter && reason != R_NO_MEMORY) {
                if (targetPort) {
                    connectTo(target, targetPort, now);
                } else {
                    finder_start(self, target);
                    cs = Cs::Probe;
                }
            } else if (esc) {
                if (reason == R_NO_MEMORY) {
                    return false;
                }
                startSearch(now);
            }
            break;
        case Cs::Stream:
            if (esc) {
                leaveStream();
                startSearch(now);
            } else if (ks.tab && ses->panel.page != PAGE_SCREEN && ses->panel.page != PAGE_RULES) {
                helpOpen = !helpOpen;
                helpPage = ses->panel.page;
                helpScroll = helpMax = 0;
                sfxClick();
            } else if (helpOpen && hasChar(ks, ';')) {
                helpScroll = std::max(0, helpScroll - 12);
            } else if (helpOpen && hasChar(ks, '.')) {
                helpScroll = std::min(helpMax, helpScroll + 12);
            }
            break;
    }
    return true;
}

// À chaque tour ; false : quitter le centre de contrôle (jamais ici : ESC, voir key())
bool update(uint32_t now) {
    runSound(now);
    switch (cs) {
        case Cs::Search: {
            int st = finder_poll(found, &nfound);
            if (st == FINDER_DONE) {
                if (nfound == 1) {
                    connectTo(found[0].ip, found[0].port, now);
                } else if (nfound > 1) {
                    listSel = 0;
                    cs = Cs::List;
                } else {
                    cs = Cs::None;
                }
            }
            break;
        }
        case Cs::Probe: {
            found_t f[FINDER_MAX];
            int n = 0;
            if (finder_poll(f, &n) == FINDER_DONE) {
                if (n > 0) {
                    connectTo(f[0].ip, f[0].port, now);
                } else {
                    reason = R_UNREACHABLE;
                    cs = Cs::Failed;
                }
            }
            break;
        }
        case Cs::Connecting: {
            int r = link_poll();
            if (r > 0) {
                linked = true;
                connecting = false;
                if (!beginSession(now)) {
                    linked = false;
                    reason = R_LOST;
                    cs = Cs::Failed;
                    break;
                }
                char ip[16];
                addr_format(target, ip);
                prefs.putString("ctl_ip", ip);
                streamSince = now;
                helpOpen = false;
                cs = Cs::Stream;
            } else if (r < 0) {
                reason = reasonConnect(r);
                cs = Cs::Failed;
            } else if ((int32_t)(now - since) > (int32_t)WAIT_MS) {
                link_close();
                reason = R_UNREACHABLE;
                cs = Cs::Failed;
            }
            break;
        }
        case Cs::Stream:
            if (linked) {
                int total = 0;
                while (total < READ_MAX) {
                    int n = link_read(rxbuf, sizeof(rxbuf));
                    if (n == 0) {
                        break;
                    }
                    if (n < 0) {
                        session_lost(ses, n == LINK_CLOSED ? SES_ERR_PEER : SES_ERR_IO);
                        break;
                    }
                    total += n;
                    if (session_feed(ses, rxbuf, (size_t)n, now) < 0) {
                        break;
                    }
                    if (ses->out_len) {  // pong tout de suite
                        if (link_write(ses->out, (int)ses->out_len) < 0) {
                            session_lost(ses, SES_ERR_IO);
                        }
                        ses->out_len = 0;
                    }
                }
                session_tick(ses, now);
                if (ses->state == SES_CLOSED) {
                    bool wasOpen = ses->frames > 0 || ses->texts > 0;
                    int err = ses->error;
                    link_close();
                    linked = false;
                    retryAt = now + RETRY_MS;
                    if (!wasOpen && notGame(err)) {
                        reason = R_NOT_GAME;  // pas le jeu : inutile d'insister
                        cs = Cs::Failed;
                    }
                }
                break;
            }
            // Reconnexion toute seule
            if (!connecting) {
                if ((int32_t)(now - retryAt) >= 0 && link_start(target, targetPort) == 0) {
                    connecting = true;
                }
                break;
            }
            {
                int r = link_poll();
                if (r > 0) {
                    connecting = false;
                    linked = beginSession(now);
                    if (!linked) {
                        retryAt = now + RETRY_MS;
                    }
                } else if (r < 0) {
                    connecting = false;
                    retryAt = now + RETRY_MS;
                }
            }
            break;
        default:
            break;
    }
    if (helpOpen && (cs != Cs::Stream || ses->panel.page != helpPage)) {
        helpOpen = false;  // la page a changé : consignes fermées
    }
    return true;
}

// ---------------------------------------------------------------- dessin : avant la partie

void drawSearch() {
    drawScreenTitle(tr(C_SEARCH_TITLE));
    text(fit(String(tr(C_NETWORK)) + netName, W - 20), 10, 30, C_TEXT);
    int p = finder_progress();
    canvas.drawRect(10, 52, W - 20, 10, C_DIM);
    canvas.fillRect(11, 53, (W - 22) * p / 100, 8, C_CYAN);
    text(String(p * 254 / 100) + " / 254", W / 2, 68, C_DIM, 1, TC_DATUM);
    drawFooter(tr(C_SEARCH_FOOTER));
}

void drawNone() {
    drawScreenTitle(tr(C_SEARCH_TITLE));
    text(fit(String(tr(C_NETWORK)) + netName, W - 20), 10, 21, C_TEXT);
    text(tr(C_NONE), 10, 35, C_YELLOW);
    int y = wrapped(tr(C_NONE_HELP), 10, 49, W - 20, C_DIM, 12);
    hint(tr(C_SEARCH_AGAIN), 10, y + 1, C_TEXT);
    drawFooter(tr(C_SEARCH_FOOTER));
}

void drawList() {
    drawScreenTitle(tr(C_LIST_TITLE));
    int n = std::min(nfound, LIST_MAX);
    for (int i = 0; i < n; i++) {
        int y = 22 + i * 24;
        bool on = i == listSel;
        char ip[16];
        addr_format(found[i].ip, ip);
        canvas.fillRoundRect(16, y, W - 32, 21, 5, on ? C_PANEL2 : C_PANEL);
        canvas.drawRoundRect(16, y, W - 32, 21, 5, on ? C_YELLOW : C_BORDER);
        text(String(tr(C_CREW)) + ip, 26, y + 4, on ? C_YELLOW : C_TEXT);
    }
    drawFooter(tr(C_LIST_FOOTER));
}

void drawAddressEntry(uint32_t now) {
    drawScreenTitle(tr(C_ADDR_TITLE));
    canvas.fillRoundRect(16, 26, W - 32, 26, 4, C_PANEL2);
    canvas.drawRoundRect(16, 26, W - 32, 26, 4, C_YELLOW);
    String shown = typed + (blink(300) ? "_" : " ");
    text(shown, W / 2, 28, C_TERM, 2, TC_DATUM);
    int y = wrapped(tr(C_ADDR_HELP), 10, 58, W - 20, C_DIM, 13);
    uint8_t ip[4];
    int st = typedStatus(ip);
    Tx *msg = st == ADDR_OTHER_NET    ? &C_ADDR_OTHER_NET
              : st == ADDR_INVALID    ? &C_ADDR_INVALID
              : st == ADDR_SELF       ? &C_ADDR_SELF
              : st == ADDR_NET_BCAST  ? &C_ADDR_NET_BCAST
                                      : nullptr;
    if (msg) {
        wrapped(tr(*msg), 10, y + 4, W - 20, st == ADDR_OTHER_NET ? C_YELLOW : C_RED, 13);
    }
    drawFooter(tr(C_ADDR_FOOTER));
}

void drawConnecting(uint32_t now) {
    drawScreenTitle(tr(C_CONN_TITLE));
    char ip[16];
    addr_format(target, ip);
    text(ip, W / 2, 32, C_TERM, 2, TC_DATUM);
    String wait = tr(C_CONN_WAIT);
    text(wait, W / 2, 70, blink() ? C_ORANGE : C_DIM, 1, TC_DATUM);
    drawFooter(tr(C_CANCEL_FOOTER));
}

void drawFailed() {
    drawScreenTitle(tr(C_ERR_TITLE));
    int y = 26;
    if (reason != R_NO_MEMORY) {
        char ip[16];
        addr_format(target, ip);
        text(ip, W / 2, 26, C_TERM, 2, TC_DATUM);
        y = 62;
    }
    Tx *msg = reason == R_REFUSED     ? &C_ERR_REFUSED
              : reason == R_NOT_GAME  ? &C_ERR_NOT_GAME
              : reason == R_NO_SOCKET ? &C_ERR_NO_SOCKET
              : reason == R_LOST      ? &C_LOST
              : reason == R_NO_MEMORY ? &C_ERR_NO_MEMORY
                                      : &C_ERR_UNREACHABLE;
    wrapped(tr(*msg), 10, y, W - 20, C_TEXT, 13);
    drawFooter(tr(reason == R_NO_MEMORY ? C_BACK_FOOTER : C_ERR_FOOTER));
}

// ---------------------------------------------------------------- dessin : la partie

// Bandeau du haut : l'oxygène, puis les 4 énigmes (en cours en orange, finies en vert)
void drawControlHud(uint32_t now, int num) {
    const panel_t &p = ses->panel;
    uint32_t rem = panel_remaining(&p, now);
    canvas.fillRect(0, 0, W, 15, C_PANEL);
    canvas.drawFastHLine(0, 15, W, C_BORDER);
    text("O₂", 3, 2, C_CYAN);
    int bw = 52;
    canvas.drawRect(20, 3, bw + 2, 9, C_DIM);
    int fill = (int)((uint64_t)bw * std::min(rem, GAME_MS) / GAME_MS);
    uint16_t bc = rem > GAME_MS / 2 ? C_GREEN : (rem > GAME_MS / 5 ? C_ORANGE : C_RED);
    canvas.fillRect(21, 4, fill, 7, bc);
    text(fmtTime(rem), 102, 2, rem < 60000 ? C_RED : C_TEXT, 1, TC_DATUM);
    if (p.flashing && now - p.flash_start < 1500) {
        text("-10 s", 120, 2, C_RED);
    }
    int x = W - 4 * 13 - 2;
    for (int i = 0; i < 4; i++) {
        bool done = i < num - 1;
        bool cur = i == num - 1;
        canvas.fillRect(x + i * 13, 1, 11, 13, cur ? C_ORANGE : done ? rgb(30, 90, 45) : C_PANEL2);
        text(String(i + 1), x + i * 13 + 6, 2, cur ? C_BLACK : done ? C_GREEN : C_DIM, 1, TC_DATUM);
    }
}

void drawRulesPage() {
    canvas.fillScreen(C_SPACE);
    drawStars(H, false);
    canvas.fillRoundRect(3, 3, W - 6, H - 6, 5, C_PANEL);
    canvas.drawRoundRect(3, 3, W - 6, H - 6, 5, C_BORDER);
    text(L(P_RULES), 10, 8, C_ORANGE);
    text(L(P_TEAM), W - 10, 8, C_CYAN, 1, TR_DATUM);
    int y = wrapped(L(P_R1), 10, 22, W - 18, C_YELLOW, 12);
    y = wrapped(L(P_R2), 10, y + 2, W - 18, C_TEXT, 12);
    y = wrapped(L(P_R3), 10, y + 2, W - 18, C_RED, 12);
    wrapped(L(P_R4), 10, y + 2, W - 18, C_DIM, 12);
}

void drawTabFooter() {
    drawFooter(L(P_TAB));
}

void drawLampPage(uint32_t now) {
    canvas.fillScreen(C_SPACE);
    drawControlHud(now, 1);
    text(fit(L(P_T1), W - 8), 4, 19, C_ORANGE);
    drawMorseTable(33, 12);
    drawTabFooter();
}

void drawPicrossPage(uint32_t now) {
    const panel_t &p = ses->panel;
    canvas.fillScreen(C_SPACE);
    drawControlHud(now, 2);
    const int cell = 14;
    const int gx = 42;
    const int gy = 46;
    canvas.setFont(&fonts::Font0);
    for (int i = 0; i < PIC_N; i++) {
        const pic_clues_t &cc = p.pic_cols[i];
        uint16_t ccol = panel_pic_ok(&p, false, i) ? C_GREEN : C_TEXT;
        for (int k = 0; k < cc.n; k++) {
            text(String(cc.v[k]), gx + i * cell + cell / 2, gy - 3 - (cc.n - k) * 9, ccol, 1, TC_DATUM);
        }
        const pic_clues_t &rc = p.pic_rows[i];
        String s;
        for (int k = 0; k < rc.n; k++) {
            s += (k ? " " : "") + String(rc.v[k]);
        }
        text(s, gx - 5, gy + i * cell + 4, panel_pic_ok(&p, true, i) ? C_GREEN : C_TEXT, 1, TR_DATUM);
    }
    canvas.setFont(&fonts::efontJA_12);
    for (int r = 0; r < PIC_N; r++) {
        for (int c = 0; c < PIC_N; c++) {
            int x = gx + c * cell;
            int y = gy + r * cell;
            canvas.fillRect(x, y, cell, cell, p.pic_grid[r * PIC_N + c] ? C_ORANGE : rgb(30, 34, 52));
            canvas.drawRect(x, y, cell + 1, cell + 1, C_BORDER);
        }
    }
    if (p.pic_cursor >= 0) {
        int x = gx + p.pic_cursor % PIC_N * cell;
        int y = gy + p.pic_cursor / PIC_N * cell;
        canvas.drawRect(x - 1, y - 1, cell + 3, cell + 3, C_YELLOW);
        canvas.drawRect(x, y, cell + 1, cell + 1, C_YELLOW);
    }
    String head, tail;
    splitTitle(L(P_T2), head, tail);
    const int px = 124;
    text(head, px, 19, C_ORANGE);
    int y = wrapped(tail, px, 33, W - px - 4, C_TEXT, 13);
    wrapped(L(P_GREEN), px, y + 7, W - px - 4, C_DIM, 13);
    drawTabFooter();
}

void drawSoundPage(uint32_t now) {
    canvas.fillScreen(C_SPACE);
    drawControlHud(now, 3);
    text(fit(L(P_T3), W - 8), 4, 19, C_ORANGE);
    wrapped(L(P_Q3), 4, 36, 140, C_TEXT, 13);
    const int cx = 186;
    const int cy = 70;
    canvas.fillRoundRect(cx - 32, cy - 32, 64, 64, 6, C_PANEL2);
    canvas.drawRoundRect(cx - 32, cy - 32, 64, 64, 6, C_BORDER);
    canvas.fillRect(cx - 18, cy - 7, 10, 14, C_GREY);
    canvas.fillTriangle(cx - 8, cy - 7, cx + 4, cy - 17, cx + 4, cy + 17, C_GREY);
    canvas.fillTriangle(cx - 8, cy + 7, cx - 8, cy - 7, cx + 4, cy + 17, C_GREY);
    uint16_t col = panel_speaker_on(&ses->panel, now) ? C_YELLOW : C_DGREY;
    canvas.fillArc(cx + 4, cy, 9, 11, -40, 40, col);
    canvas.fillArc(cx + 4, cy, 16, 18, -40, 40, col);
    canvas.fillArc(cx + 4, cy, 23, 25, -40, 40, col);
    drawTabFooter();
}

// Planètes de la page (sans thème), dans un carré de 16 px agrandi s fois
const uint16_t PLANET_COL[8] = {rgb(160, 160, 168), rgb(232, 200, 120), rgb(60, 120, 220), rgb(210, 80, 45),
                                rgb(216, 168, 120), rgb(230, 204, 140), rgb(140, 220, 230), rgb(60, 90, 210)};

void drawPlanet(int k, int x, int y) {
    int c = 8;
    canvas.fillCircle(x + c, y + c, k == 5 ? 4 : 6, PLANET_COL[k]);
    if (k == 5) {
        canvas.drawEllipse(x + c, y + c, 7, 2, rgb(200, 176, 112));
    } else if (k == 2) {
        canvas.fillEllipse(x + c - 1, y + c - 1, 2, 2, rgb(70, 180, 80));
    } else if (k == 4) {
        canvas.drawFastHLine(x + 3, y + c - 2, 10, rgb(160, 100, 60));
        canvas.drawFastHLine(x + 3, y + c + 2, 10, rgb(160, 100, 60));
    }
}

// Image de la case k (0 à 7) : dessin du thème (message M) ou planète
void drawPicture(int k, int x, int y) {
    const panel_t &p = ses->panel;
    if (!p.theme) {
        drawPlanet(k, x, y);
        return;
    }
    if (p.ranks[k] >= THEME_PICS) {
        return;
    }
    const uint8_t *px = p.pics[p.ranks[k]];
    for (int r = 0; r < THEME_SIZE; r++) {
        for (int c = 0; c < THEME_SIZE; c++) {
            uint8_t v = px[r * THEME_SIZE + c];
            if (v) {
                uint32_t rgbv = p.palette[v - 1];
                canvas.drawPixel(x + c, y + r, rgb(rgbv >> 16, rgbv >> 8 & 0xFF, rgbv & 0xFF));
            }
        }
    }
}

const char *pictureName(int k) {
    return ses->panel.theme ? ses->panel.names[k] : L(P_PLANETS[k]);
}

// Labyrinthe (choix M1) : la grille, et la légende des 8 images avec leurs noms
void drawMazePage(uint32_t now) {
    const panel_t &p = ses->panel;
    canvas.fillScreen(C_SPACE);
    drawControlHud(now, 4);
    const int cell = 20;
    const int gx = 3;
    const int gy = 19;
    for (int i = 0; i < MAZE_CELLS; i++) {
        int x = gx + i % MAZE_W * cell;
        int y = gy + i / MAZE_W * cell;
        char v = p.maze[i];
        uint16_t bg = i == p.maze_bad ? rgb(140, 25, 25) : p.maze_trail[i] ? rgb(140, 70, 15) : rgb(30, 34, 52);
        canvas.fillRect(x, y, cell, cell, bg);
        canvas.drawRect(x, y, cell + 1, cell + 1, C_BORDER);
        if (v == 'X') {  // sortie : trappe verte, comme chez l'équipage
            canvas.fillRoundRect(x + 2, y + 2, cell - 3, cell - 3, 2, rgb(20, 90, 40));
            canvas.drawRoundRect(x + 2, y + 2, cell - 3, cell - 3, 2, C_GREEN);
            canvas.fillTriangle(x + 7, y + 5, x + 7, y + cell - 5, x + 13, y + cell / 2, C_GREEN);
        } else if (v == 'E') {
            canvas.drawRoundRect(x + 2, y + 2, cell - 3, cell - 3, 2, C_DIM);
        } else if (v >= '1' && v <= '8') {
            drawPicture(v - '1', x + 3, y + 3);
        }
        if (i == p.maze_pos) {
            drawAstro(x + cell - 10, y + 1);
        }
    }
    String head, tail;
    splitTitle(L(P_T4), head, tail);
    const int px = 90;
    text(head, px, 19, C_ORANGE);
    for (int k = 0; k < 8; k++) {
        int x = px + (k / 4) * 74;
        int y = 35 + (k % 4) * 20;
        drawPicture(k, x, y);
        text(fit(pictureName(k), 54), x + 19, y + 2, C_TEXT);
    }
    drawTabFooter();
}

void drawKeypadPage(uint32_t now) {
    const panel_t &p = ses->panel;
    canvas.fillScreen(C_SPACE);
    drawControlHud(now, 5);
    String head, tail;
    splitTitle(L(P_TC), head, tail);
    int y = wrapped(tail, 4, 19, 88, C_ORANGE, 13);
    y = wrapped(L(P_SA), 4, y + 5, 88, C_TEXT, 13);
    for (int i = 0; i < 4; i++) {
        int x = 4 + i * 22;
        canvas.drawRect(x, y + 3, 20, 20, C_TERM);
        if (i < p.typed) {
            canvas.fillRect(x + 3, y + 6, 14, 14, C_TERM);
        }
    }
    for (int k = 0; k < p.ncells && k < 9; k++) {
        int x = 96 + (k % 3) * 48;
        int cy = 18 + (k / 3) * 34;
        canvas.fillRoundRect(x, cy, 46, 32, 3, C_PANEL2);
        canvas.drawRoundRect(x, cy, 46, 32, 3, C_BORDER);
        text(String(p.cells[k].letter), x + 8, cy + 10, C_YELLOW, 1, TC_DATUM);
        if (p.cells[k].sym < SYM_COUNT) {
            drawSymbol(p.cells[k].sym, x + 17, cy + 4, 2, C_TEXT);
        }
    }
    drawTabFooter();
}

// Consignes (TAB) : le texte de la page web qui ne tient pas à côté de l'énigme.
// Plus haut que l'écran (indice d'un thème) : ▲▼ le font défiler.
void drawHelp(char page) {
    const panel_t &p = ses->panel;
    canvas.fillScreen(C_SPACE);
    drawStars(H, false);
    const int top = 3;
    const int bottom = H - 17;  // au-dessus du pied de page
    canvas.fillRoundRect(3, top, W - 6, bottom - top, 5, C_PANEL);
    canvas.drawRoundRect(3, top, W - 6, bottom - top, 5, C_BORDER);
    canvas.setClipRect(4, top + 2, W - 8, bottom - top - 4);
    const int lh = 12;
    const int y0 = top + 4 - helpScroll;
    int y;
    switch (page) {
        case PAGE_LAMP:
            y = wrapped(L(P_T1), 10, y0, W - 20, C_ORANGE, lh);
            y = wrapped(L(P_P1), 10, y + 3, W - 20, C_TEXT, lh);
            y = wrapped(L(P_Q1), 10, y + 3, W - 20, C_DIM, lh);
            break;
        case PAGE_PICROSS:
            y = wrapped(L(P_T2), 10, y0, W - 20, C_ORANGE, lh);
            y = wrapped(L(P_P2), 10, y + 3, W - 20, C_TEXT, lh);
            break;
        case PAGE_SOUND:
            y = wrapped(L(P_T3), 10, y0, W - 20, C_ORANGE, lh);
            y = wrapped(L(P_P3), 10, y + 3, W - 20, C_TEXT, lh);
            y = wrapped(L(P_Q3), 10, y + 3, W - 20, C_DIM, lh);
            break;
        case PAGE_MAZE:
            y = wrapped(L(P_T4), 10, y0, W - 20, C_ORANGE, lh);
            y = wrapped(L(p.theme ? P_P4T : P_P4), 10, y + 3, W - 20, C_TEXT, lh);
            y = wrapped(p.theme ? String(p.hint) : String(L(P_H4)), 10, y + 3, W - 20, C_YELLOW, lh);
            y = wrapped(L(P_E4), 10, y + 3, W - 20, C_DIM, lh);
            break;
        default:  // PAGE_KEYPAD
            y = wrapped(L(P_TC), 10, y0, W - 20, C_ORANGE, lh);
            y = wrapped(L(P_PC), 10, y + 3, W - 20, C_TEXT, lh);
            break;
    }
    canvas.clearClipRect();
    helpMax = std::max(0, y + helpScroll - (bottom - 3));
    helpScroll = std::min(helpScroll, helpMax);
    drawFooter(L(helpMax > 0 ? P_TAB_SCROLL : P_TAB_CLOSE));
}

// Copie de l'écran de l'équipage, et le bandeau de fin défilé ici (45 px/s, comme le jeu)
void drawCopy(uint32_t now) {
    canvas.pushImage(0, 0, W, H, reinterpret_cast<const lgfx::rgb565_t *>(ses->screen));
    if (!band[0]) {
        return;
    }
    canvas.fillRect(0, bandY, W, bandH, C_PANEL);
    canvas.drawFastHLine(0, bandY, W, C_BORDER);
    canvas.drawFastHLine(0, bandY + bandH - 1, W, C_BORDER);
    String info(band);
    int len = canvas.textWidth(info) + W / 2;
    int x = W - (int)((uint64_t)(now - bandStart) * WIN_INFO_SPEED / 1000 % (W + len));
    text(info, x, bandY + 2, C_TEXT);
}

void drawStream(uint32_t now) {
    const panel_t &p = ses->panel;
    if (helpOpen) {
        drawHelp(helpPage);
    } else {
        switch (p.page) {
            case PAGE_RULES: drawRulesPage(); break;
            case PAGE_LAMP: drawLampPage(now); break;
            case PAGE_PICROSS: drawPicrossPage(now); break;
            case PAGE_SOUND: drawSoundPage(now); break;
            case PAGE_MAZE: drawMazePage(now); break;
            case PAGE_KEYPAD: drawKeypadPage(now); break;
            default: drawCopy(now); break;
        }
        if (panel_flash_age(&p, now) >= 0) {  // mauvaise réponse : cadre rouge
            canvas.drawRect(0, 0, W, H, C_RED);
            canvas.drawRect(1, 1, W - 2, H - 2, C_RED);
            canvas.drawRect(2, 2, W - 4, H - 4, C_RED);
        }
    }
    // En bas à gauche : connexion en cours, version différente de l'équipage
    int y = H - 13;
    if (!linked || ses->state == SES_HANDSHAKE) {
        String wait = L(C_CONN_WAIT);
        canvas.fillRect(0, y - 2, canvas.textWidth(wait) + 8, 15, C_SPACE);
        text(wait, 4, y, C_ORANGE);
        y -= 15;
    }
    if (ses->bad_panels > 0) {
        String old = fit(L(C_OLD_GAME), W - 8);
        canvas.fillRect(0, y - 2, canvas.textWidth(old) + 8, 15, C_SPACE);
        text(old, 4, y, C_RED);
    }
    if (now - streamSince < HINT_MS) {  // comment quitter, au début
        String leave = L(C_LEAVE);
        int w = runWidth(leave.c_str(), leave.length()) + 10;
        canvas.fillRoundRect(W - w - 2, 2, w, 15, 3, C_PANEL);
        canvas.drawRoundRect(W - w - 2, 2, w, 15, 3, C_BORDER);
        hint(leave.c_str(), W - 7, 3, C_TEXT, TR_DATUM);
    }
}

void draw(uint32_t now) {
    switch (cs) {
        case Cs::Search: drawSearch(); break;
        case Cs::None: drawNone(); break;
        case Cs::List: drawList(); break;
        case Cs::Address: drawAddressEntry(now); break;
        case Cs::Probe:
        case Cs::Connecting: drawConnecting(now); break;
        case Cs::Failed: drawFailed(); break;
        case Cs::Stream: drawStream(now); break;
    }
}

}  // namespace controle
