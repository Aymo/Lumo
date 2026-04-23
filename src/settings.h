#pragma once

#include <stdint.h>

// Persistente gebruikersinstellingen in NVS (flash). Waardes worden bij boot
// geladen en met een korte debounce naar flash teruggeschreven om slijtage
// van het flash-geheugen te voorkomen.
class Settings {
 public:
  void begin();                          // laad uit NVS
  void tick(uint32_t now_ms);            // flush dirty waardes na debounce

  uint8_t  volume() const      { return _volume; }
  uint16_t eyeColor() const    { return _eye_color; }
  uint8_t  language() const    { return _language; }
  uint8_t  orientation() const { return _orientation; }

  void setVolume(uint8_t v);
  void setEyeColor(uint16_t c);
  void setLanguage(uint8_t l);
  void setOrientation(uint8_t r);

 private:
  uint8_t  _volume      = 85;
  uint16_t _eye_color   = 0xFD20;   // LUMO_ORANGE
  uint8_t  _language    = 0;        // 0 = NL, zie i18n::Lang
  uint8_t  _orientation = 1;        // 0..3; matcht Arduino_GFX setRotation

  bool     _dirty          = false;
  uint32_t _last_change_ms = 0;

  void flush();
};

extern Settings settings;
