#pragma once

#include "eye_engine.h"

// Set emoties die Lumo kan tonen. Uitbreidbaar: voeg toe in de enum,
// en geef hem een preset in emotion.cpp::presetFor().
enum class Emotion : uint8_t {
  NEUTRAL,
  HAPPY,
  ANGRY,
  SURPRISED,
  SLEEPY,
  SCARED,
  LOVE,      // hart-ogen, kort getoond bij USB aansluiten
};

const char* emotionName(Emotion e);

// Geeft de EyeParams-preset voor een emotie.
EyeParams presetFor(Emotion e);

// Beheert de huidige emotie en praat met de EyeEngine.
class EmotionController {
 public:
  explicit EmotionController(EyeEngine& engine);

  // Zet een nieuwe emotie; EyeEngine interpoleert ernaartoe.
  // `transition_rate`: 0.05 = trage morph, 0.4 = snelle snap.
  void setEmotion(Emotion e, float transition_rate = 0.12f);

  // Herapplyeer de huidige emotie; handig als een globale preset-parameter
  // is gewijzigd (bv de eye-kleur) en we de new baseline willen toepassen.
  void refresh(float transition_rate = 0.4f);

  Emotion current() const { return _current; }

 private:
  EyeEngine& _engine;
  Emotion    _current = Emotion::NEUTRAL;
};

// Globale eye-kleur. Alle presets gebruiken deze via baseNeutral().
// Default is oranje (LUMO_ORANGE).
void     setGlobalEyeColor(uint16_t rgb565);
uint16_t getGlobalEyeColor();
