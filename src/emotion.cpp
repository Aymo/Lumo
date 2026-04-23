#include "emotion.h"

namespace {

// Globale eye-kleur; aanpasbaar door de color picker.
uint16_t g_eye_color = LUMO_ORANGE;

// Gedeelde "neutrale" basis voor ovaal-ogen zonder pupil.
EyeParams baseNeutral() {
  EyeParams p;
  p.radius_x     = 45;
  p.radius_y     = 65;
  p.eye_spacing  = 170;
  p.vertical_off = 0;
  p.color_eye    = g_eye_color;
  return p;
}

}  // namespace

void     setGlobalEyeColor(uint16_t c) { g_eye_color = c; }
uint16_t getGlobalEyeColor()           { return g_eye_color; }

const char* emotionName(Emotion e) {
  switch (e) {
    case Emotion::NEUTRAL:   return "NEUTRAL";
    case Emotion::HAPPY:     return "HAPPY";
    case Emotion::ANGRY:     return "ANGRY";
    case Emotion::SURPRISED: return "SURPRISED";
    case Emotion::SLEEPY:    return "SLEEPY";
    case Emotion::SCARED:    return "SCARED";
    case Emotion::LOVE:      return "LOVE";
  }
  return "?";
}

// Expressies bij pupil-loze ogen moeten het puur van vorm en oogleden hebben.
// De belangrijkste hefbomen:
//   - radius_x / radius_y : "staand" (hoog smal = 0-vorm) of "liggend" (breed kort)
//   - openness_top/_bot   : welk deel van de ovaal zichtbaar is
//   - vertical_off        : blik naar boven/onder simuleren door de ogen te schuiven
//   - eye_spacing         : verder uit elkaar bij verrassing, dichter bij bang
EyeParams presetFor(Emotion e) {
  EyeParams p = baseNeutral();
  switch (e) {
    case Emotion::NEUTRAL:
      // 0-vorm, ontspannen open.
      break;

    case Emotion::HAPPY:
      // Klassieke ^_^ : onderlid flink omhoog zodat alleen de bovenkant van de
      // ovaal zichtbaar is, wat een glimlach-boog oplevert.
      p.openness_top = 1.0f;
      p.openness_bot = 0.40f;
      p.vertical_off = -8;
      break;

    case Emotion::ANGRY:
      // Ovale vorm blijft grotendeels intakt; de "frons" komt uit schuine
      // brow-driehoeken die naar de neus wijzen. Rood oogwit benadrukt boos.
      p.radius_y       = 60;
      p.openness_top   = 1.0f;
      p.openness_bot   = 1.0f;
      p.brow_intensity = 0.55f;
      p.color_eye      = 0xF800;    // puur rood
      break;

    case Emotion::SURPRISED:
      // Groter en iets ronder, ogen wijd open en verder uit elkaar.
      p.radius_x     = 55;
      p.radius_y     = 72;
      p.eye_spacing  = 186;
      p.openness_top = 1.0f;
      p.openness_bot = 1.0f;
      break;

    case Emotion::SLEEPY:
      // Zwaar bovenlid en onderlid iets omhoog; ogen zakken iets.
      p.openness_top = 0.28f;
      p.openness_bot = 0.82f;
      p.vertical_off = 8;
      break;

    case Emotion::SCARED:
      // Ogen poppen open: breder, hoger, wijder uit elkaar, iets omhoog (alert).
      p.radius_x     = 62;
      p.radius_y     = 88;
      p.eye_spacing  = 205;
      p.openness_top = 1.0f;
      p.openness_bot = 1.0f;
      p.vertical_off = -8;
      break;

    case Emotion::LOVE:
      // Hart-ogen: bitmap-icoon wordt 100x100 getekend per oog; radius_*
      // is irrelevant voor de vorm maar blijft gezet voor consistente layout.
      p.radius_x     = 50;
      p.radius_y     = 50;
      p.eye_spacing  = 160;
      p.openness_top = 1.0f;
      p.openness_bot = 1.0f;
      p.heart_shape  = true;
      break;
  }
  return p;
}

EmotionController::EmotionController(EyeEngine& engine) : _engine(engine) {}

void EmotionController::setEmotion(Emotion e, float transition_rate) {
  _current = e;
  _engine.setTarget(presetFor(e), transition_rate);
}

void EmotionController::refresh(float transition_rate) {
  _engine.setTarget(presetFor(_current), transition_rate);
}
