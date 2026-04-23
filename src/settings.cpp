#include "settings.h"

#include <Arduino.h>
#include <Preferences.h>

namespace {
Preferences       prefs;
constexpr char    NS[]           = "lumo";
constexpr uint32_t DEBOUNCE_MS   = 1500;
}

Settings settings;

void Settings::begin() {
  prefs.begin(NS, false);
  _volume      = prefs.getUChar ("vol",   85);
  _eye_color   = prefs.getUShort("color", 0xFD20);
  _language    = prefs.getUChar ("lang",  0);
  _orientation = prefs.getUChar ("rot",   1);
  prefs.end();
  Serial.printf("[settings] geladen: vol=%u color=0x%04X lang=%u rot=%u\n",
                _volume, _eye_color, _language, _orientation);
}

void Settings::tick(uint32_t now_ms) {
  if (_dirty && now_ms - _last_change_ms >= DEBOUNCE_MS) {
    flush();
  }
}

void Settings::flush() {
  prefs.begin(NS, false);
  prefs.putUChar ("vol",   _volume);
  prefs.putUShort("color", _eye_color);
  prefs.putUChar ("lang",  _language);
  prefs.putUChar ("rot",   _orientation);
  prefs.end();
  _dirty = false;
  Serial.printf("[settings] opgeslagen: vol=%u color=0x%04X lang=%u rot=%u\n",
                _volume, _eye_color, _language, _orientation);
}

void Settings::setVolume(uint8_t v) {
  if (v == _volume) return;
  _volume = v;
  _dirty = true;
  _last_change_ms = millis();
}

void Settings::setEyeColor(uint16_t c) {
  if (c == _eye_color) return;
  _eye_color = c;
  _dirty = true;
  _last_change_ms = millis();
}

void Settings::setLanguage(uint8_t l) {
  if (l == _language) return;
  _language = l;
  _dirty = true;
  _last_change_ms = millis();
}

void Settings::setOrientation(uint8_t r) {
  r &= 0x03;   // clamp 0..3
  if (r == _orientation) return;
  _orientation = r;
  _dirty = true;
  _last_change_ms = millis();
}
