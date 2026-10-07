// Simulateur PC : mémoire NVS gardée en RAM le temps de la session.
#pragma once
#include <Arduino.h>

#include <map>

class Preferences {
public:
    bool begin(const char *, bool) { return true; }
    bool isKey(const char *k) { return _v.count(k) > 0; }
    String getString(const char *k, const String &def) { return isKey(k) ? String(_v[k]) : def; }
    void putString(const char *k, const String &v) { _v[k] = v.c_str(); }
    uint32_t getUInt(const char *k, uint32_t def) { return isKey(k) ? (uint32_t)std::stoul(_v[k]) : def; }
    void putUInt(const char *k, uint32_t v) { _v[k] = std::to_string(v); }
    uint8_t getUChar(const char *k, uint8_t def) { return isKey(k) ? (uint8_t)std::stoi(_v[k]) : def; }
    void putUChar(const char *k, uint8_t v) { _v[k] = std::to_string(v); }
    bool getBool(const char *k, bool def) { return isKey(k) ? _v[k] == "1" : def; }
    void putBool(const char *k, bool v) { _v[k] = v ? "1" : "0"; }

    static std::map<std::string, std::string> &store();  // réglages passés en ligne de commande

private:
    std::map<std::string, std::string> &_v = store();
};
