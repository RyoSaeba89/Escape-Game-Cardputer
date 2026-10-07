// Simulateur PC : M5Cardputer réduit à ce que le jeu utilise. L'écran est le
// vrai M5GFX (fenêtre SDL), le clavier vient du clavier du PC, le son est muet.
#pragma once
#include <Arduino.h>
#include <M5GFX.h>

#include <memory>
#include <vector>

namespace m5 {
using board_t = m5gfx::board_t;
}

struct Point2D_t {
    int x = 0;
    int y = 0;
    bool operator==(const Point2D_t &o) const { return x == o.x && y == o.y; }
};

class KeyboardReader {
public:
    virtual ~KeyboardReader() = default;
    virtual void begin() {}
    virtual void update() {}

protected:
    std::vector<Point2D_t> _key_list;
};

struct KeysState {
    std::vector<char> word;
    bool tab = false, fn = false, shift = false, ctrl = false, opt = false, alt = false;
    bool del = false, enter = false, space = false;
};

class Keyboard_Class {
public:
    void begin() {}
    void begin(std::unique_ptr<KeyboardReader>) {}
    void updateKeyList() {}
    void updateKeysState();  // sim_main.cpp : un appui par tour, puis un relâchement
    bool isChange() const { return _change; }
    bool isPressed() const { return _pressed; }
    KeysState keysState() const { return _state; }

private:
    KeysState _state;
    bool _change = false;
    bool _pressed = false;
    bool _release = false;
};

class Speaker_Class {
public:
    bool begin() { return true; }
    void setVolume(uint8_t) {}
    bool tone(float, uint32_t, int = -1, bool = true) { return true; }
    void stop() {}
    void stop(uint8_t) {}
    bool isPlaying(uint8_t) const { return false; }
    bool playRaw(const int16_t *, size_t, uint32_t, bool, uint32_t, int, bool) { return true; }
    void setChannelVolume(uint8_t, uint8_t) {}
};

struct M5Cardputer_Class {
    M5GFX Display;
    Keyboard_Class Keyboard;
    Speaker_Class Speaker;
    template <typename C>
    void begin(const C &, bool) { Display.init(); }
    void update() {}
};
extern M5Cardputer_Class M5Cardputer;

struct M5_Class {
    struct config_t {};
    config_t config() const { return {}; }
    m5::board_t getBoard() const { return m5::board_t::board_M5Cardputer; }
};
extern M5_Class M5;
