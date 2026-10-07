// Simulateur PC : contrôleur de clavier du Cardputer ADV, jamais utilisé (le
// simulateur se présente comme un Cardputer simple).
#pragma once
#include <cstdint>

class Adafruit_TCA8418 {
public:
    bool begin() { return false; }
    void matrix(uint8_t, uint8_t) {}
    void flush() {}
    uint8_t getEvent() { return 0; }
};
