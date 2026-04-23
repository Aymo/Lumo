#pragma once

#include <stdint.h>

struct PowerStatus {
  uint8_t  percent;       // 0..100 (255 = onbekend)
  uint16_t voltage_mv;    // batterij voltage in mV
  bool     is_charging;
  bool     vbus_present;  // USB aangesloten
  bool     valid;
};

class Power {
 public:
  bool begin();
  PowerStatus read();

 private:
  bool _ready = false;
};

extern Power power_mgr;
