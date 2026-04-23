#include "touch.h"

#include <Arduino.h>
#include <Wire.h>

#include "TouchDrvCSTXXX.hpp"
#include "pin_config.h"

namespace {

TouchDrvCSTXXX cst;
constexpr uint8_t  TOUCH_I2C_ADDR = 0x5A;

// Tap-criteria: tik = kort indrukken, korte tijd, weinig beweging.
constexpr uint32_t MAX_TAP_DURATION_MS  = 250;
constexpr int16_t  MAX_TAP_DRIFT_PX     = 20;

// Swipe-criteria: duidelijke richting, voldoende afstand, niet te traag.
constexpr int16_t  MIN_SWIPE_PX         = 80;
constexpr uint32_t MAX_SWIPE_DURATION_MS = 700;

}  // namespace

Touch touch;

bool Touch::begin() {
  // Haal de touch-chip uit reset. Zonder deze puls antwoordt CST9217 niet op I2C
  // omdat de pin bij boot low is.
  pinMode(TP_RESET, OUTPUT);
  digitalWrite(TP_RESET, LOW);
  delay(10);
  digitalWrite(TP_RESET, HIGH);
  delay(50);

  if (!cst.begin(Wire, TOUCH_I2C_ADDR, IIC_SDA, IIC_SCL)) {
    Serial.printf("[touch] CST9217 niet gevonden op 0x%02X\n", TOUCH_I2C_ADDR);
    return false;
  }
  cst.setMaxCoordinates(LCD_WIDTH, LCD_HEIGHT);
  // Geen hardware-mirror: rotatie doen we zelf in tick() zodat die matcht met
  // de canvas-rotatie. Dat houdt de transformaties op 1 plek.

  _ready = true;
  Serial.println("[touch] OK");
  return true;
}

// Transformeer ruwe touch-coords (panel-frame) naar canvas-frame, gegeven de
// huidige canvas-rotatie. Spiegels Arduino_GFX' interne rotatie-mapping.
static void applyRotation(int16_t& x, int16_t& y, uint8_t rotation) {
  const int16_t rx = x, ry = y;
  switch (rotation & 3) {
    case 0: /* identiteit */                  break;
    case 1: x = LCD_HEIGHT - 1 - ry; y = rx;  break;
    case 2: x = LCD_WIDTH  - 1 - rx; y = LCD_HEIGHT - 1 - ry; break;
    case 3: x = ry;                  y = LCD_WIDTH  - 1 - rx; break;
  }
}

void Touch::tick(uint32_t now_ms) {
  if (!_ready) return;

  int16_t xs[1], ys[1];
  const uint8_t n = cst.getPoint(xs, ys, 1);
  const bool touched = (n > 0);
  if (touched) applyRotation(xs[0], ys[0], _rotation);

  // Down-edge.
  if (touched && !_prev_touched) {
    _down_ms      = now_ms;
    _last_down_ms = now_ms;
    _down_x       = xs[0];
    _down_y       = ys[0];
    _last_x       = xs[0];
    _last_y       = ys[0];
  }

  // Positie bijwerken tijdens touch zodat we bij up-edge de eindpositie kennen.
  if (touched) {
    _last_x = xs[0];
    _last_y = ys[0];
  }

  // Up-edge: beoordeel of het een tap of swipe was.
  if (!touched && _prev_touched) {
    const uint32_t dur = now_ms - _down_ms;
    const int16_t  dx  = _last_x - _down_x;
    const int16_t  dy  = _last_y - _down_y;
    const int16_t  adx = dx < 0 ? -dx : dx;
    const int16_t  ady = dy < 0 ? -dy : dy;

    // Swipe heeft voorrang op tap: dominante richting + voldoende afstand.
    const bool swipe_candidate =
        dur <= MAX_SWIPE_DURATION_MS &&
        (adx >= MIN_SWIPE_PX || ady >= MIN_SWIPE_PX);

    if (swipe_candidate && adx > ady && adx > 2 * ady) {
      _pending_event = (dx < 0) ? TouchEvent::SWIPE_LEFT : TouchEvent::SWIPE_RIGHT;
      _tap_state     = TapState::IDLE;
    } else if (swipe_candidate && ady > adx && ady > 2 * adx) {
      _pending_event = (dy < 0) ? TouchEvent::SWIPE_UP : TouchEvent::SWIPE_DOWN;
      _tap_state     = TapState::IDLE;
    } else if (dur <= MAX_TAP_DURATION_MS &&
               adx <= MAX_TAP_DRIFT_PX && ady <= MAX_TAP_DRIFT_PX) {
      // Echte tap.
      if (_tap_state == TapState::IDLE) {
        _tap_state        = TapState::WAITING_FOR_DOUBLE;
        _first_tap_ms     = now_ms;
        _last_tap_point   = {_last_x, _last_y, true};
      } else {
        _pending_event  = TouchEvent::DOUBLE_TAP;
        _tap_state      = TapState::IDLE;
      }
    }
  }

  // Timeout op single-tap: als er binnen het venster geen tweede tap komt, emit SINGLE.
  if (_tap_state == TapState::WAITING_FOR_DOUBLE &&
      now_ms - _first_tap_ms > _double_tap_window_ms) {
    _pending_event = TouchEvent::SINGLE_TAP;
    _tap_state     = TapState::IDLE;
  }

  _prev_touched = touched;
}
