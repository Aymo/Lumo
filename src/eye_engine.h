#pragma once

#include <Arduino_GFX_Library.h>
#include <stdint.h>

// Classic orange RGB565 (255, 165, 0).
static constexpr uint16_t LUMO_ORANGE = 0xFD20;

// Parameters die het uiterlijk van de (ovale, pupil-loze) ogen beschrijven.
// De EyeEngine lerpt tussen current en target, zodat overgangen vloeiend zijn.
struct EyeParams {
  // Ovaalvorm (pixels): horizontale en verticale straal.
  // Voor een "0"-vorm: radius_y > radius_x.
  int16_t  radius_x      = 45;
  int16_t  radius_y      = 65;

  // Afstand tussen de twee oogcentra (pixels).
  int16_t  eye_spacing   = 170;

  // Verticale positie-offset t.o.v. schermmidden (negatief = omhoog).
  int16_t  vertical_off  = 0;

  // Openheid van de ogen: 0.0 = lid volledig dicht, 1.0 = volledig open.
  // Expressies komen vooral van hier + uit de ovaal-verhouding.
  float    openness_top  = 1.0f;
  float    openness_bot  = 1.0f;

  // Schuine wenkbrauw over de bovenkant van het oog. 0 = geen brow,
  // 1 = diep-schuine brow (outer hoog, inner laag -> wijzend naar de neus).
  float    brow_intensity = 0.0f;

  // Kijkrichting (-1..+1). Zonder pupillen schuiven we het oog zelf in die
  // richting om "kijken naar links/rechts/boven/onder" te simuleren.
  float    look_x        = 0.0f;
  float    look_y        = 0.0f;

  // Vervang ovaal door hart-vorm (bv. bij LOVE-emotie bij USB-plug).
  bool     heart_shape   = false;

  // Kleuren (RGB565).
  uint16_t color_eye     = LUMO_ORANGE;
  uint16_t color_bg      = 0x0000;
};

class EyeEngine {
 public:
  EyeEngine(Arduino_GFX* gfx);

  void begin();

  // Zet nieuwe doelparameters. EyeEngine interpoleert er vloeiend naartoe.
  void setTarget(const EyeParams& target, float lerp_rate = 0.15f);

  // Direct naar een toestand springen zonder animatie (bijv. bij boot).
  void snapTo(const EyeParams& p);

  // Trigger een knipper-animatie (~140ms dicht-open).
  void blink();

  // Auto-blink aan/uit. Aan = willekeurig knipperen elke 3-6 sec.
  void setAutoBlink(bool enabled) { _auto_blink = enabled; }

  // Korte trilanimatie: random x/y offset op de oogcentra.
  // amplitude_px: maximale verschuiving per frame
  // duration_ms: totale duur; amplitude neemt lineair af naar 0.
  void shake(int16_t amplitude_px, uint32_t duration_ms);

  // Update alleen de kijkrichting van het doel; wordt via de lerp
  // vloeiend naar dit punt bewogen.
  void setLookTarget(float x, float y);

  // Update animatie-state (doet geen render).
  void tick(uint32_t now_ms);

  // Teken huidige state naar de gfx (typisch een Canvas die daarna geflusht wordt).
  void render();

  const EyeParams& current() const { return _current; }
  const EyeParams& target()  const { return _target;  }

 private:
  Arduino_GFX* _gfx;

  EyeParams _current;
  EyeParams _target;
  float     _lerp_rate = 0.15f;

  bool      _blink_active     = false;
  uint32_t  _blink_start_ms   = 0;
  bool      _auto_blink       = true;
  uint32_t  _next_autoblink_ms = 0;

  uint32_t  _last_tick_ms = 0;

  // Shake state
  int16_t   _shake_amplitude   = 0;
  uint32_t  _shake_start_ms    = 0;
  uint32_t  _shake_duration_ms = 0;

  void lerpCurrentToward(const EyeParams& t, float alpha);
  void scheduleNextAutoBlink(uint32_t now_ms);
  void drawEye(int16_t cx, int16_t cy, const EyeParams& p, bool is_left);
};
