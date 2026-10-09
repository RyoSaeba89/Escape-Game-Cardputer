// Simulateur PC : mode multijoueur joué « à vide ». Réseaux fictifs, connexion
// réussie au bout de 1,5 s (« Mauvais mot de passe » : refusée), un navigateur
// et un appareil Explorer3 qui se connectent tout seuls.
#include "../src/diffusion.h"

namespace mirror {
namespace {
uint32_t scanAt = 0;
uint32_t connectAt = 0;
uint32_t serverAt = 0;
uint32_t apAt = 0;
bool ap = false;
String ssid;
}  // namespace

void startScan() {
    scanAt = millis();
    ap = false;
}

Scan pollScan(std::vector<WifiNet> &out) {
    uint32_t t = millis() - scanAt;
    if (t < 1500) {
        return Scan::Running;
    }
    out = {{"MaBox-1234", -48, false}, {"Livebox-5F2A", -66, false}, {"Mauvais mot de passe", -74, false},
           {"FreeWifi_secure", -80, false}};
    if (t < 3000) {
        return Scan::Partial;
    }
    out.push_back({"Voisin-du-dessous", -88, true});  // trouvé par la seconde passe
    return Scan::Done;
}

void connect(const String &s, const String &) {
    ssid = s;
    connectAt = millis();
}

Link link() {
    if (millis() - connectAt < 1500) {
        return Link::Connecting;
    }
    return ssid == "Mauvais mot de passe" ? Link::BadPassword : Link::Connected;
}

Link failure() {
    return Link::NotFound;
}

int attempt() {
    return 1 + (millis() - connectAt) / 4000;
}

bool startAccessPoint() {
    ap = true;
    apAt = millis();
    return true;
}

bool accessPoint() {
    return ap;
}

int apClients() {
    return ap && millis() - apAt > 6000 ? 1 : 0;
}

String wifiQrText() {
    return String("WIFI:T:WPA;S:") + AP_SSID + ";P:" + AP_PASS + ";;";
}

String ipAddress() {
    return ap ? "192.168.4.1" : "192.168.1.42";
}

bool startServer(const uint16_t *, int, int) {
    serverAt = millis();
    return true;
}

int clientCount() {
    return millis() - serverAt > 4000 ? 1 : 0;
}

void lockScreen() {}
void unlockScreen() {}
void sendTone(uint32_t, uint16_t, uint16_t, uint8_t) {}
void sendStop(uint32_t, uint8_t) {}
void sendRumble(uint32_t) {}
void setPanel(const String &) {}
void setBanner(const String &, int, int) {}
void setLanguage(uint8_t) {}
void setSymbols(const char *) {}

}  // namespace mirror
