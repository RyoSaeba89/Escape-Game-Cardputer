// Simulateur PC : le strict nécessaire d'Arduino pour compiler src/main.cpp
// (String, millis, random…). Voir sim/LISEZMOI.md.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>

#ifndef PI
#define PI 3.14159265358979323846
#endif
#define PROGMEM

uint32_t millis();
void delay(uint32_t ms);
uint32_t esp_random();
void randomSeed(unsigned long seed);
long random(long howbig);
long random(long howsmall, long howbig);

// Chaîne Arduino réduite aux fonctions utilisées par le jeu
class String {
public:
    String() = default;
    String(const char *s) : _s(s ? s : "") {}
    String(const std::string &s) : _s(s) {}
    explicit String(char c) : _s(1, c) {}
    template <typename T, typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, char>::value, int>::type = 0>
    explicit String(T v) : _s(std::to_string(v)) {}

    const char *c_str() const { return _s.c_str(); }
    operator const char *() const { return _s.c_str(); }  // pour drawString(), textWidth()…
    unsigned int length() const { return (unsigned int)_s.size(); }
    bool isEmpty() const { return _s.empty(); }

    char operator[](int i) const { return _s[i]; }
    char operator[](unsigned int i) const { return _s[i]; }
    char operator[](unsigned long i) const { return _s[i]; }
    char operator[](unsigned long long i) const { return _s[i]; }

    int indexOf(char c, unsigned int from = 0) const {
        size_t p = _s.find(c, from);
        return p == std::string::npos ? -1 : (int)p;
    }
    String substring(unsigned int from, unsigned int to) const {
        if (from > _s.size()) return String();
        return String(_s.substr(from, std::min<size_t>(to, _s.size()) - from));
    }
    String substring(unsigned int from) const { return substring(from, length()); }
    void remove(unsigned int index) { if (index < _s.size()) _s.erase(index); }
    void remove(unsigned int index, unsigned int count) { if (index < _s.size()) _s.erase(index, count); }

    String &operator+=(const String &o) { _s += o._s; return *this; }
    String &operator+=(const char *o) { _s += o; return *this; }
    String &operator+=(char c) { _s += c; return *this; }

    friend String operator+(const String &a, const String &b) { return String(a._s + b._s); }
    friend String operator+(const String &a, const char *b) { return String(a._s + b); }
    friend String operator+(const char *a, const String &b) { return String(std::string(a) + b._s); }
    friend bool operator==(const String &a, const String &b) { return a._s == b._s; }
    friend bool operator==(const String &a, const char *b) { return a._s == b; }
    friend bool operator!=(const String &a, const String &b) { return a._s != b._s; }
    friend bool operator!=(const String &a, const char *b) { return a._s != b; }

private:
    std::string _s;
};
