#pragma once

#include <time.h>
#include <stdint.h>

// Dunne wrapper om de onboard PCF85063 RTC.
// Houdt systeem-tijd (time.h) en hardware-RTC in sync.
class ClockRtc {
 public:
  // Init. Als de RTC een geldige tijd bevat, wordt die naar het ESP32-systeem
  // gezet. Zo niet, dan behoudt het systeem de default-tijd.
  bool begin();

  // Schrijft de huidige ESP32-systeemtijd naar de RTC zodat die een power-cycle
  // overleeft.
  bool saveSystemTimeToRtc();

 private:
  bool _ready = false;
};

extern ClockRtc clock_rtc;
