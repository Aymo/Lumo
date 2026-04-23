#pragma once

#include <stdint.h>
#include <stddef.h>

// Multi-language support. Iedere taal woont in een eigen bestand in src/lang/
// en publiceert een const LangTable. i18n.cpp houdt een registry van alle
// tabellen en switcht er live tussen; alle UI-code haalt strings op via i18n::t().
//
// Een nieuwe taal toevoegen:
//   1. Kopieer lang_en.cpp naar lang_xx.cpp en pas strings + code + name aan.
//   2. Voeg `extern const i18n::LangTable LANG_XX;` toe in i18n.cpp.
//   3. Voeg &LANG_XX toe aan de TABLES-array en verhoog LANG_COUNT.
// De UI pakt de nieuwe taal automatisch op in de language-picker.

namespace i18n {

// Stringkeys die de UI kan opvragen. Volgorde bepaalt de index in de
// strings-array van iedere LangTable, dus NIET herschikken zonder ook alle
// lang_*.cpp-bestanden bij te werken.
enum class T : uint16_t {
  // Hoofd-menu tegels
  MENU_BATTERY,
  MENU_VOLUME,
  MENU_COLOR,
  MENU_SETTINGS,
  // Instellingen-submenu tegels
  MENU_TIME,
  MENU_LANGUAGE,
  MENU_ORIENTATION,
  // Schermtitels
  TITLE_SETTINGS,
  TITLE_BATTERY,
  TITLE_VOLUME,
  TITLE_TIME,
  TITLE_LANGUAGE,
  TITLE_ORIENTATION,
  // Hints onderaan schermen
  HINT_SWIPE_BACK,
  HINT_SWIPE_SAVE,
  HINT_DRAG_RING,
  HINT_TAP_CENTER_BACK,
  // Batterij-status
  STATUS_NO_DATA,
  STATUS_CHARGING,
  STATUS_USB,
  // Deep sleep
  SLEEP_ZZZ,
  // Tijd-setter veldnamen
  TIME_HOUR,
  TIME_MINUTE,
  TIME_DAY,
  TIME_MONTH,
  TIME_YEAR,
  COUNT
};

enum class Lang : uint8_t { NL = 0, EN, ES };

// Totaal aantal geregistreerde talen. Ophogen wanneer er een nieuwe
// LangTable aan TABLES wordt toegevoegd in i18n.cpp.
constexpr uint8_t LANG_COUNT = 3;

struct LangTable {
  const char* code;    // ISO-achtige korte code, bv. "NL"
  const char* name;    // Menselijke naam in de doeltaal zelf, bv. "Nederlands"
  const char* strings[static_cast<size_t>(T::COUNT)];
};

// Initialiseer en kies een taal. Aanroepen NA settings.begin() in setup().
void begin(Lang l);

// Wissel live van taal; volgende frame rendert al in de nieuwe taal.
void setLanguage(Lang l);

Lang        current();
const char* code(Lang l);
const char* name(Lang l);

// Geef de vertaling op in de huidig actieve taal. Als een specifieke key
// ontbreekt in de huidige tabel (nullptr), valt hij automatisch terug op EN
// en daarna NL; wanneer alles mist retourneert hij "??".
const char* t(T key);

}  // namespace i18n
