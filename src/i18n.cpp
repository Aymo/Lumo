#include "i18n.h"

// De taaltabellen zelf leven ieder in hun eigen bestand onder src/lang/.
// Hier registreren we ze in een statische array, en houden we de huidige
// keuze bij via een pointer.
extern const i18n::LangTable LANG_NL;
extern const i18n::LangTable LANG_EN;
extern const i18n::LangTable LANG_ES;

namespace {

// Registry. De volgorde moet matchen met de i18n::Lang enum zodat een stored
// uint8_t uit NVS direct naar de juiste tabel wijst.
const i18n::LangTable* const TABLES[i18n::LANG_COUNT] = {
  &LANG_NL,
  &LANG_EN,
  &LANG_ES,
};

const i18n::LangTable* g_current = &LANG_NL;

}  // namespace

namespace i18n {

void begin(Lang l) { setLanguage(l); }

void setLanguage(Lang l) {
  const uint8_t idx = static_cast<uint8_t>(l);
  if (idx < LANG_COUNT) g_current = TABLES[idx];
}

Lang current() {
  for (uint8_t i = 0; i < LANG_COUNT; i++) {
    if (TABLES[i] == g_current) return static_cast<Lang>(i);
  }
  return Lang::NL;
}

const char* code(Lang l) {
  const uint8_t idx = static_cast<uint8_t>(l);
  return (idx < LANG_COUNT) ? TABLES[idx]->code : "??";
}

const char* name(Lang l) {
  const uint8_t idx = static_cast<uint8_t>(l);
  return (idx < LANG_COUNT) ? TABLES[idx]->name : "?";
}

const char* t(T key) {
  const uint16_t idx = static_cast<uint16_t>(key);
  if (idx >= static_cast<uint16_t>(T::COUNT)) return "??";

  const char* s = g_current->strings[idx];
  if (s) return s;
  // Fallback-ketting: ontbrekende string trekt eerst uit EN, dan uit NL.
  s = LANG_EN.strings[idx];
  if (s) return s;
  s = LANG_NL.strings[idx];
  return s ? s : "??";
}

}  // namespace i18n
