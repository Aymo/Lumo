#pragma once

#include <stdint.h>

// Bewegingsgebeurtenissen die Lumo detecteert.
// Voor nu alleen PICKED_UP; SHAKEN / TAP / TILT komen later zodra we ze nodig hebben.
enum class MotionEvent : uint8_t {
  NONE,
  PICKED_UP,
};

struct MotionSample {
  float    ax, ay, az;   // g (eenheden van 9.81 m/s^2)
  float    magnitude;    // sqrt(ax^2 + ay^2 + az^2)
  uint32_t timestamp_ms;
};

// Dunne wrapper om de QMI8658 6-assige IMU met een event-API.
// Sampled de accelerometer op ~50 Hz, bijhoudt een running average van de magnitude
// en produceert PICKED_UP wanneer de momentane magnitude flink afwijkt van dat gemiddelde.
class Imu {
 public:
  // Init. Aanroepen *nadat* Wire.begin() is uitgevoerd.
  // Returns false als de QMI8658 niet reageert.
  bool begin();

  // Pollt de sensor en update interne state / events.
  void tick(uint32_t now_ms);

  // Haal het laatste (nog niet verbruikte) event. NONE als er niets is.
  MotionEvent peekEvent() const { return _pending_event; }
  void        consumeEvent()    { _pending_event = MotionEvent::NONE; }

  const MotionSample& lastSample() const { return _last_sample; }
  float averageMagnitude() const { return _magnitude_ema; }

  // Gevoeligheid: delta t.o.v. gemiddelde magnitude die PICKED_UP triggert.
  // 0.20g vangt normale hand-pickups, 0.35g alleen echt rukken.
  void setPickupThreshold(float g) { _pickup_threshold_g = g; }

 private:
  MotionSample _last_sample{};
  MotionEvent  _pending_event = MotionEvent::NONE;

  float    _magnitude_ema        = 1.0f;
  float    _pickup_threshold_g   = 0.20f;
  uint32_t _next_sample_ms       = 0;
  uint32_t _event_cooldown_until = 0;
  bool     _has_baseline         = false;
  bool     _ready                = false;
};
