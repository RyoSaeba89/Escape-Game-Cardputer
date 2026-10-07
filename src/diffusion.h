// Mode diffusion : l'écran et le son du jeu sont recopiés en direct dans le
// navigateur d'un PC sur le même Wi-Fi (page web servie par le Cardputer).
#pragma once
#include <Arduino.h>

#include <vector>

namespace mirror {

struct WifiNet {
    String ssid;
    int rssi;
    bool open;
};

// Wi-Fi
void startScan();
bool scanDone(std::vector<WifiNet> &out);  // true une fois la recherche finie (liste triée)
void connect(const String &ssid, const String &pass);
bool connected();
String address();  // "http://192.168.1.42"

// Serveur : page web (port 80) + flux écran/son (WebSocket, port 81)
void startServer(const uint16_t *screen, int w, int h);
int clientCount();

// À appeler autour du dessin de l'écran (le flux le lit depuis l'autre cœur)
void lockScreen();
void unlockScreen();

// Son joué par le PC (heures en millis() du Cardputer)
void sendTone(uint32_t at, uint16_t freq, uint16_t dur, uint8_t ch);
void sendStop(uint32_t at, uint8_t ch);  // ch = 255 : tous les canaux
void sendRumble(uint32_t at);           // grondement du décollage

}  // namespace mirror
