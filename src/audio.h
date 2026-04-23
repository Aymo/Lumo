#pragma once

#include <stdint.h>

// Audio-subsysteem voor Lumo: ES8311 codec + I2S driver.
// Geluiden worden procedureel gegenereerd (geen WAV files) zodat we geen SD
// of flashgeheugen nodig hebben.

class Audio {
 public:
  // Init I2S + ES8311 + speaker amp. Aanroepen NA Wire.begin().
  bool begin();

  // Plant een "gilletje" in de audio-task. Non-blocking: de render-loop
  // blijft gewoon draaien terwijl het geluidje wordt afgespeeld.
  void playScream();

  bool ready() const { return _ready; }

  // Volumebesturing: 0..100 (ES8311 DAC volume). Live verstelbaar.
  void    setVolume(uint8_t v);
  uint8_t getVolume() const { return _volume; }

  // Klap-detectie via de I2S mics. Event wordt geset door een achtergrond-
  // task die continu de audio-input samplet. Net als IMU: peek + consume.
  bool clapPending();
  void consumeClap();

  // Laatste piek-amplitude die de mic-task zag (voor tuning van de drempel).
  int32_t lastMicPeak() const { return _last_mic_peak; }

  // Pas de detectie-drempel aan tijdens runtime (0..32767). Default 8000.
  void setClapThreshold(int32_t t) { _clap_threshold = t; }

  // Velden voor de mic-task; moeten public omdat de task in anonymous
  // namespace zit in audio.cpp en geen friend-access kan krijgen.
  volatile bool    _clap_pending    = false;
  volatile int32_t _last_mic_peak   = 0;
  int32_t          _clap_threshold  = 8000;

 private:
  bool    _ready  = false;
  uint8_t _volume = 85;    // defaultvolume bij boot
};

extern Audio audio;
