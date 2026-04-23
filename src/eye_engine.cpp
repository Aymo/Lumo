#include "eye_engine.h"

#include <Arduino.h>
#include <math.h>

#include "heart_icon.h"

namespace {

inline float clampf(float v, float lo, float hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline int16_t lerpi(int16_t a, int16_t b, float t) {
  return static_cast<int16_t>(lroundf(lerpf(a, b, t)));
}

inline float easeInOut(float t) {
  t = clampf(t, 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

constexpr uint32_t BLINK_DURATION_MS = 220;

// Globale verticale basis-offset: ogen hoger dan schermmidden zodat ze
// op "ooghoogte" voelen. Per emotie ligt bovenop deze offset nog een kleine
// preset-afhankelijke vertical_off (bv SLEEPY zakt iets, HAPPY gaat iets omhoog).
constexpr int16_t EYE_BASE_Y_OFFSET = -30;

}  // namespace

EyeEngine::EyeEngine(Arduino_GFX* gfx) : _gfx(gfx) {}

void EyeEngine::begin() {
  _last_tick_ms = millis();
  scheduleNextAutoBlink(_last_tick_ms);
}

void EyeEngine::snapTo(const EyeParams& p) {
  _current = p;
  _target = p;
}

void EyeEngine::setTarget(const EyeParams& t, float lerp_rate) {
  _target = t;
  _lerp_rate = clampf(lerp_rate, 0.01f, 1.0f);
}

void EyeEngine::blink() {
  _blink_active = true;
  _blink_start_ms = millis();
}

void EyeEngine::shake(int16_t amplitude_px, uint32_t duration_ms) {
  _shake_amplitude   = amplitude_px;
  _shake_start_ms    = millis();
  _shake_duration_ms = duration_ms;
}

void EyeEngine::setLookTarget(float x, float y) {
  if (x < -1.0f) x = -1.0f; if (x > 1.0f) x = 1.0f;
  if (y < -1.0f) y = -1.0f; if (y > 1.0f) y = 1.0f;
  _target.look_x = x;
  _target.look_y = y;
  // Snappy saccade: snellere lerp dan de default emotie-transities, zodat
  // de blik-verandering er scherp uitziet.
  _lerp_rate = 0.25f;
}

void EyeEngine::scheduleNextAutoBlink(uint32_t now_ms) {
  // Knipper elke 1.5-3.5 sec -- net wat vaker dan menselijk gemiddelde, zodat
  // blinks ook samenvallen met look-bewegingen op verschillende momenten.
  _next_autoblink_ms = now_ms + 1500 + (esp_random() % 2001);
}

void EyeEngine::tick(uint32_t now_ms) {
  if (_auto_blink && !_blink_active && now_ms >= _next_autoblink_ms) {
    blink();
  }

  // Frame-rate onafhankelijke lerp: corrigeer stap voor verstreken tijd.
  uint32_t dt_ms = now_ms - _last_tick_ms;
  _last_tick_ms = now_ms;
  if (dt_ms == 0) dt_ms = 1;
  float alpha = _lerp_rate * (dt_ms / 16.67f);
  alpha = clampf(alpha, 0.0f, 1.0f);

  lerpCurrentToward(_target, alpha);

  // Knipper-curve: 0..0.5 dichtgaan, 0.5..1 openen.
  // Bij peak komen de oogleden naar elkaar toe tot net voor het midden,
  // zodat er een smalle horizontale stripe (breedte van het oog) zichtbaar
  // blijft = typisch "oog dicht" cartoon-look.
  if (_blink_active) {
    uint32_t elapsed = now_ms - _blink_start_ms;
    if (elapsed >= BLINK_DURATION_MS) {
      _blink_active = false;
      scheduleNextAutoBlink(now_ms);
    } else {
      float t = static_cast<float>(elapsed) / BLINK_DURATION_MS;
      float closed = (t < 0.5f) ? easeInOut(t * 2.0f) : easeInOut((1.0f - t) * 2.0f);

      // Oogleden "ontmoeten" elkaar op openness ~0.52: elk lid bedekt 48%
      // van de eye-hoogte, gap in het midden = 4% van de hoogte (~5 px op
      // een radius_y van 65). Dat is een duidelijk streepje.
      const float LINE_MEET_OPENNESS = 0.52f;

      // Clamp: nooit OPENER maken dan het target. Belangrijk voor emoties
      // waar een lid al verder dicht staat dan 0.52 (bv. SLEEPY).
      const float peak_top = _target.openness_top < LINE_MEET_OPENNESS
                             ? _target.openness_top : LINE_MEET_OPENNESS;
      const float peak_bot = _target.openness_bot < LINE_MEET_OPENNESS
                             ? _target.openness_bot : LINE_MEET_OPENNESS;

      _current.openness_top = lerpf(_target.openness_top, peak_top, closed);
      _current.openness_bot = lerpf(_target.openness_bot, peak_bot, closed);
    }
  }
}

void EyeEngine::lerpCurrentToward(const EyeParams& t, float a) {
  _current.radius_x       = lerpi(_current.radius_x,      t.radius_x,      a);
  _current.radius_y       = lerpi(_current.radius_y,      t.radius_y,      a);
  _current.eye_spacing    = lerpi(_current.eye_spacing,   t.eye_spacing,   a);
  _current.vertical_off   = lerpi(_current.vertical_off,  t.vertical_off,  a);
  _current.brow_intensity = lerpf(_current.brow_intensity, t.brow_intensity, a);
  _current.look_x         = lerpf(_current.look_x,         t.look_x,         a);
  _current.look_y         = lerpf(_current.look_y,         t.look_y,         a);

  if (!_blink_active) {
    _current.openness_top = lerpf(_current.openness_top, t.openness_top, a);
    _current.openness_bot = lerpf(_current.openness_bot, t.openness_bot, a);
  }

  _current.color_eye   = t.color_eye;
  _current.color_bg    = t.color_bg;
  _current.heart_shape = t.heart_shape;
}

void EyeEngine::drawEye(int16_t cx, int16_t cy, const EyeParams& p, bool is_left) {
  // Hart-vorm via bitmap: 100x100 PNG met alpha, geimporteerd als RGB565 +
  // 1-bit alpha mask. We blitten pixel-voor-pixel en skippen transparante.
  if (p.heart_shape) {
    const int16_t x0 = cx - heart_W / 2;
    const int16_t y0 = cy - heart_H / 2;
    for (int16_t py = 0; py < heart_H; py++) {
      for (int16_t px = 0; px < heart_W; px++) {
        const int idx  = py * heart_W + px;
        const uint8_t byte = heart_mask[idx >> 3];
        const uint8_t bit  = 0x80 >> (idx & 7);
        if (byte & bit) {
          _gfx->drawPixel(x0 + px, y0 + py, heart_rgb565[idx]);
        }
      }
    }
    return;
  }

  // Capsule-vorm: rechte verticale flanken, halve-cirkel caps boven/onder.
  // Cornerradius = radius_x zorgt dat de hoeken volledige halve cirkels worden
  // en de middelste sectie strakke verticale lijnen heeft.
  const int16_t x = cx - p.radius_x;
  const int16_t y = cy - p.radius_y;
  const int16_t w = p.radius_x * 2;
  const int16_t h = p.radius_y * 2;
  int16_t r = p.radius_x;
  const int16_t r_max = (w < h ? w : h) / 2;
  if (r > r_max) r = r_max;   // clampen wanneer radius_x >= radius_y
  _gfx->fillRoundRect(x, y, w, h, r, p.color_eye);

  // Oogleden: zwarte rechthoeken die de ovaal van boven/onder afdekken.
  // Bounding box van de ovaal: cy-radius_y .. cy+radius_y, cx-radius_x .. cx+radius_x.
  // +2 marge horizontaal tegen ellipse-stair-stepping aan de rand.
  const int16_t eye_top    = cy - p.radius_y;
  const int16_t eye_bot    = cy + p.radius_y;
  const int16_t diameter_y = p.radius_y * 2;
  const int16_t rect_x     = cx - p.radius_x - 2;
  const int16_t rect_w     = p.radius_x * 2 + 4;

  const int16_t top_h = static_cast<int16_t>(lroundf((1.0f - p.openness_top) * diameter_y));
  if (top_h > 0) {
    _gfx->fillRect(rect_x, eye_top, rect_w, top_h, p.color_bg);
  }

  const int16_t bot_h = static_cast<int16_t>(lroundf((1.0f - p.openness_bot) * diameter_y));
  if (bot_h > 0) {
    _gfx->fillRect(rect_x, eye_bot - bot_h, rect_w, bot_h, p.color_bg);
  }

  // Schuine wenkbrauw: zwarte driehoek die diagonaal over de bovenkant valt.
  // Voor links-oog: van outer (links) hoog naar inner (rechts) laag -> driehoek
  //   covert de upper-right hoek (inner boven-corner dalend).
  // Voor rechts-oog: spiegelbeeld; driehoek covert upper-left hoek.
  if (p.brow_intensity > 0.01f) {
    const int16_t top   = eye_top - 2;
    const int16_t left  = rect_x;
    const int16_t right = rect_x + rect_w;
    const int16_t reach = static_cast<int16_t>(lroundf(p.brow_intensity * p.radius_y * 1.3f));
    if (is_left) {
      _gfx->fillTriangle(left,  top,
                         right, top,
                         right, top + reach,
                         p.color_bg);
    } else {
      _gfx->fillTriangle(left,  top,
                         right, top,
                         left,  top + reach,
                         p.color_bg);
    }
  }
}

void EyeEngine::render() {
  _gfx->fillScreen(_current.color_bg);

  // Shake: amplitude decayt lineair naar 0 over de duration.
  int16_t shake_dx = 0, shake_dy = 0;
  if (_shake_amplitude > 0 && _shake_duration_ms > 0) {
    const uint32_t elapsed = millis() - _shake_start_ms;
    if (elapsed < _shake_duration_ms) {
      const float remaining = 1.0f - (float)elapsed / _shake_duration_ms;
      const int16_t amp = (int16_t)(_shake_amplitude * remaining);
      if (amp > 0) {
        shake_dx = (int16_t)(esp_random() % (amp * 2 + 1)) - amp;
        shake_dy = (int16_t)(esp_random() % (amp * 2 + 1)) - amp;
      }
    } else {
      _shake_amplitude = 0;
    }
  }

  // Kijkrichting: beide ogen als geheel verschuiven in de look-richting.
  // Zonder pupillen is dit de beste manier om "kijken naar X" te tonen.
  const int16_t look_dx = (int16_t)lroundf(_current.look_x * 32.0f);
  const int16_t look_dy = (int16_t)lroundf(_current.look_y * 20.0f);

  const int16_t cx   = _gfx->width()  / 2 + shake_dx + look_dx;
  const int16_t cy   = _gfx->height() / 2 + EYE_BASE_Y_OFFSET
                       + _current.vertical_off + shake_dy + look_dy;
  const int16_t half = _current.eye_spacing / 2;

  drawEye(cx - half, cy, _current, /*is_left=*/true);
  drawEye(cx + half, cy, _current, /*is_left=*/false);
}
