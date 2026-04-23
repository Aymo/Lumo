#pragma once

#include <stdint.h>

enum class TouchEvent : uint8_t {
  NONE,
  SINGLE_TAP,
  DOUBLE_TAP,
  SWIPE_LEFT,
  SWIPE_RIGHT,
  SWIPE_UP,
  SWIPE_DOWN,
};

// Ruwe touch-positie, voor gebruik in UI-schermen die op tap-locatie reageren.
struct TouchPoint {
  int16_t  x;
  int16_t  y;
  bool     valid;
};

// Dunne wrapper om de CST9217 touch controller.
// Detecteert single-tap en double-tap (uitgesteld om beide te kunnen onderscheiden).
class Touch {
 public:
  bool begin();
  void tick(uint32_t now_ms);

  // Zet de rotatie zodat touch-coords matchen met canvas-coords
  // (dezelfde rotatie die je op de canvas gebruikt).
  void setRotation(uint8_t rotation) { _rotation = rotation; }

  TouchEvent peekEvent() const { return _pending_event; }
  void       consumeEvent()    { _pending_event = TouchEvent::NONE; }

  // Locatie van de laatste tap (alleen zinvol direct na SINGLE_TAP event).
  TouchPoint lastTapPoint() const { return _last_tap_point; }

  // Tijdstip van de laatste "touch down" overgang; handig voor idle-detectie.
  uint32_t   lastTouchMs() const { return _last_down_ms; }

  // Live touch-state: vraag continu uit in de render-loop voor drag-gestures
  // (zoals een color-picker ring die tijdens slepen live de kleur update).
  bool    isTouched() const { return _prev_touched; }
  int16_t touchX()    const { return _last_x; }
  int16_t touchY()    const { return _last_y; }

  // Instelbaar: dubbel-tik venster (tweede tap binnen deze ms = DOUBLE_TAP).
  void setDoubleTapWindow(uint32_t ms) { _double_tap_window_ms = ms; }

 private:
  enum class TapState : uint8_t { IDLE, WAITING_FOR_DOUBLE };

  TouchEvent _pending_event = TouchEvent::NONE;
  TapState   _tap_state     = TapState::IDLE;
  TouchPoint _last_tap_point{0, 0, false};

  bool       _prev_touched  = false;
  uint32_t   _down_ms       = 0;
  uint32_t   _last_down_ms  = 0;
  int16_t    _down_x        = 0;
  int16_t    _down_y        = 0;
  int16_t    _last_x        = 0;
  int16_t    _last_y        = 0;

  uint32_t   _first_tap_ms  = 0;
  uint32_t   _double_tap_window_ms = 350;

  bool       _ready    = false;
  uint8_t    _rotation = 0;
};

extern Touch touch;
