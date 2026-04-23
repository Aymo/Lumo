#include "imu.h"

#include <Arduino.h>
#include <Wire.h>
#include <math.h>

#include "SensorQMI8658.hpp"
#include "pin_config.h"

namespace {

SensorQMI8658 qmi;

constexpr uint32_t SAMPLE_INTERVAL_MS = 20;     // 50 Hz
constexpr uint32_t EVENT_COOLDOWN_MS  = 1500;   // min tijd tussen opeenvolgende events
constexpr float    EMA_ALPHA          = 0.03f;  // langzame baseline; pieken mogen er uitsteken

}  // namespace

bool Imu::begin() {
  if (!qmi.begin(Wire, QMI8658_L_SLAVE_ADDRESS, IIC_SDA, IIC_SCL) &&
      !qmi.begin(Wire, QMI8658_H_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    Serial.println("[imu] QMI8658 niet gevonden");
    return false;
  }

  qmi.configAccelerometer(
      SensorQMI8658::ACC_RANGE_4G,
      SensorQMI8658::ACC_ODR_250Hz,
      SensorQMI8658::LPF_MODE_0);
  qmi.enableAccelerometer();

  _ready = true;
  Serial.println("QMI8658 OK");
  return true;
}

void Imu::tick(uint32_t now_ms) {
  if (!_ready || now_ms < _next_sample_ms) return;
  _next_sample_ms = now_ms + SAMPLE_INTERVAL_MS;

  if (!qmi.getDataReady()) return;

  IMUdata acc;
  if (!qmi.getAccelerometer(acc.x, acc.y, acc.z)) return;

  const float mag = sqrtf(acc.x * acc.x + acc.y * acc.y + acc.z * acc.z);

  _last_sample = {acc.x, acc.y, acc.z, mag, now_ms};

  // Seed de running average bij de allereerste sample zodat we niet
  // direct na boot een false-positive triggeren.
  if (!_has_baseline) {
    _magnitude_ema = mag;
    _has_baseline = true;
    return;
  }

  const float delta = fabsf(mag - _magnitude_ema);

  // Event-detectie voordat we het EMA updaten; anders "eet" de EMA het piekje op.
  if (now_ms >= _event_cooldown_until && delta >= _pickup_threshold_g) {
    _pending_event = MotionEvent::PICKED_UP;
    _event_cooldown_until = now_ms + EVENT_COOLDOWN_MS;
  }

  _magnitude_ema += EMA_ALPHA * (mag - _magnitude_ema);
}
