// Mode multijoueur : la page web du centre de contrôle, servie par le Cardputer
// au navigateur d'un PC, d'une télé ou d'un téléphone, sur le Wi-Fi de la box ou
// sur le réseau Explorer3 créé par le Cardputer. Elle reçoit le son du jeu, sa
// propre page pendant les énigmes, et sinon la copie en direct de l'écran.
#pragma once
#include <Arduino.h>

#include <vector>

namespace mirror {

// Réseau créé par le Cardputer quand il n'y a pas de box (WPA2 : 8 caractères au moins)
constexpr const char *AP_SSID = "Explorer3";
constexpr const char *AP_PASS = "Explorer3";
constexpr const char *HOST_NAME = "explorer3.local";  // mDNS, en plus de l'adresse IP

struct WifiNet {
    String ssid;
    int rssi;
    bool open;
};

// ---------------------------------------------------------------- Wi-Fi de la box

// Recherche en deux passes (active, puis passive pour les réseaux qui répondent
// mal) dont les résultats sont réunis. Un refus du pilote est retenté tout seul.
enum class Scan { Running, Partial, Done, Failed };
void startScan();
// Partial : première liste, la seconde passe continue ; Done : liste complète ;
// Failed : recherche impossible malgré les nouveaux essais. Listes triées par signal.
Scan pollScan(std::vector<WifiNet> &out);

enum class Link { Connecting, Connected, BadPassword, NotFound, NoAnswer };
void connect(const String &ssid, const String &pass);
Link link();     // BadPassword dès que le mot de passe est refusé deux fois
Link failure();  // cause probable après le délai : NotFound ou NoAnswer
int attempt();   // numéro de l'essai en cours (le pilote réessaie de lui-même)

// ---------------------------------------------------------------- réseau Explorer3

bool startAccessPoint();
bool accessPoint();  // true si le Cardputer est en point d'accès
int apClients();     // appareils connectés au réseau Explorer3
String wifiQrText(); // texte du QR code qui fait rejoindre le réseau

String ipAddress();  // "192.168.1.42" (box) ou "192.168.4.1" (Explorer3)

// ---------------------------------------------------------------- serveur

// Page web (port 80) + flux écran/son (WebSocket, port 81) + nom explorer3.local.
// false si la mémoire manque (rien n'est démarré). À rappeler après chaque
// changement de réseau : le nom explorer3.local est alors annoncé de nouveau.
bool startServer(const uint16_t *screen, int w, int h);
int clientCount();

// À appeler autour du dessin de l'écran (le flux le lit depuis l'autre cœur)
void lockScreen();
void unlockScreen();

// Son joué par le navigateur (heures en millis() du Cardputer)
void sendTone(uint32_t at, uint16_t freq, uint16_t dur, uint8_t ch);
void sendStop(uint32_t at, uint8_t ch);  // ch = 255 : tous les canaux
void sendRumble(uint32_t at);           // grondement du décollage

// Page du centre de contrôle : "0" = copie de l'écran, "R" = règles,
// "1" à "4" = énigme, "C" = table du clavier codé. Pour "1" à "4" et "C", suivi de
// ",restant_ms,erreurs,données" : picross "grille,lignes,colonnes,curseur" (grille =
// 25 chiffres 0/1, indices « 3.1/1.1.1/… », curseur = case 0 à 24), clavier codé "saisis,table" (table =
// lettre + n° de symbole en hexa). Voir updatePanel() dans main.cpp.
void setPanel(const String &text);

// Bandeau qui défile sur l'écran de fin : la page le dessine et le fait défiler
// elle-même, et les lignes y à y + h - 1 ne passent plus dans la copie de l'écran.
// Texte vide : plus de bandeau, ces lignes sont de nouveau envoyées.
void setBanner(const String &text, int y, int h);

void setLanguage(uint8_t lang);       // 0 = français, 1 = anglais (textes de la page)
void setSymbols(const char *js);      // dessins des symboles (/sym.js), gardé tel quel

}  // namespace mirror
