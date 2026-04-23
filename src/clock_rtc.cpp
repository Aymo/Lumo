#include "clock_rtc.h"

#include <Arduino.h>
#include <Wire.h>
#include <sys/time.h>

#include "SensorPCF85063.hpp"
#include "pin_config.h"

namespace {
SensorPCF85063 rtc;
}

ClockRtc clock_rtc;

bool ClockRtc::begin() {
  if (!rtc.begin(Wire, IIC_SDA, IIC_SCL)) {
    Serial.println("[rtc] PCF85063 niet gevonden");
    return false;
  }
  _ready = true;

  // Lees RTC en zet systeemtijd, mits de RTC een plausibel jaar heeft.
  RTC_DateTime dt = rtc.getDateTime();
  const uint16_t y = dt.getYear();
  if (y >= 2020 && y <= 2099) {
    struct tm tm_v = {};
    tm_v.tm_year = y              - 1900;
    tm_v.tm_mon  = dt.getMonth()  - 1;
    tm_v.tm_mday = dt.getDay();
    tm_v.tm_hour = dt.getHour();
    tm_v.tm_min  = dt.getMinute();
    tm_v.tm_sec  = dt.getSecond();
    time_t t = mktime(&tm_v);
    struct timeval tv = {t, 0};
    settimeofday(&tv, nullptr);
    Serial.printf("[rtc] geladen: %04u-%02u-%02u %02u:%02u:%02u\n",
                  y, dt.getMonth(), dt.getDay(),
                  dt.getHour(), dt.getMinute(), dt.getSecond());
  } else {
    Serial.printf("[rtc] leeg of ongeldig (jaar=%u); systeem houdt default\n", y);
  }
  return true;
}

bool ClockRtc::saveSystemTimeToRtc() {
  if (!_ready) return false;
  time_t now = time(nullptr);
  struct tm tm_v;
  localtime_r(&now, &tm_v);
  rtc.setDateTime(
      tm_v.tm_year + 1900,
      tm_v.tm_mon  + 1,
      tm_v.tm_mday,
      tm_v.tm_hour,
      tm_v.tm_min,
      tm_v.tm_sec);
  Serial.printf("[rtc] opgeslagen: %04d-%02d-%02d %02d:%02d:%02d\n",
                tm_v.tm_year + 1900, tm_v.tm_mon + 1, tm_v.tm_mday,
                tm_v.tm_hour, tm_v.tm_min, tm_v.tm_sec);
  return true;
}
