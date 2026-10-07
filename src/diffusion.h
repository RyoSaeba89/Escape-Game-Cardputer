// Broadcast mode: the game screen and sound are mirrored live in the
// browser of a PC on the same Wi-Fi (web page served by the Cardputer).
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
bool scanDone(std::vector<WifiNet> &out);  // true once the scan is over (sorted list)
void connect(const String &ssid, const String &pass);
bool connected();
String address();  // "http://192.168.1.42"

// Server: web page (port 80) + screen/sound stream (WebSocket, port 81)
void startServer(const uint16_t *screen, int w, int h);
int clientCount();

// Call around the screen drawing (the stream reads it from the other core)
void lockScreen();
void unlockScreen();

// Sound played by the PC (times in Cardputer millis())
void sendTone(uint32_t at, uint16_t freq, uint16_t dur, uint8_t ch);
void sendStop(uint32_t at, uint8_t ch);  // ch = 255: all channels
void sendRumble(uint32_t at);           // liftoff rumble

}  // namespace mirror
