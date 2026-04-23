#include "power.h"

#include <Arduino.h>
#include <Wire.h>

#define XPOWERS_CHIP_AXP2101
#include "XPowersLib.h"

#include "pin_config.h"

namespace {
XPowersPMU pmu;
}

Power power_mgr;

bool Power::begin() {
  if (!pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, IIC_SDA, IIC_SCL)) {
    Serial.println("[power] AXP2101 niet gevonden");
    return false;
  }
  // ADC's aan zodat we batterij-voltage + percentage kunnen uitlezen.
  pmu.enableBattDetection();
  pmu.enableBattVoltageMeasure();
  pmu.enableVbusVoltageMeasure();
  pmu.enableSystemVoltageMeasure();

  _ready = true;
  Serial.println("[power] OK");
  return true;
}

PowerStatus Power::read() {
  PowerStatus s{};
  if (!_ready) return s;
  s.percent      = pmu.getBatteryPercent();
  s.voltage_mv   = pmu.getBattVoltage();
  s.is_charging  = pmu.isCharging();
  s.vbus_present = pmu.isVbusIn();
  s.valid        = true;
  return s;
}
