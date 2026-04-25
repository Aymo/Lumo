// Nederlandse vertalingen (default-taal voor Lumo).
//
// Volgorde van de strings-array MOET matchen met de i18n::T enum in i18n.h.
// Een ontbrekende entry valt automatisch terug op EN → "??" in de runtime.

#include "../i18n.h"

extern const i18n::LangTable LANG_NL = {
  "NL",
  "Nederlands",
  {
    /* MENU_BATTERY         */ "Batterij",
    /* MENU_VOLUME          */ "Volume",
    /* MENU_COLOR           */ "Kleur",
    /* MENU_SETTINGS        */ "Instellingen",
    /* MENU_TIME            */ "Tijd",
    /* MENU_LANGUAGE        */ "Taal",
    /* MENU_ORIENTATION     */ "Orientatie",
    /* TITLE_SETTINGS       */ "Instellingen",
    /* TITLE_BATTERY        */ "Batterij",
    /* TITLE_VOLUME         */ "Volume",
    /* TITLE_TIME           */ "Tijd & datum",
    /* TITLE_LANGUAGE       */ "Taal",
    /* TITLE_ORIENTATION    */ "Orientatie",
    /* HINT_SWIPE_BACK      */ "swipe -> terug",
    /* HINT_SWIPE_SAVE      */ "swipe -> opslaan",
    /* HINT_DRAG_RING       */ "sleep de ring",
    /* HINT_TAP_CENTER_BACK */ "tik midden -> terug",
    /* STATUS_NO_DATA       */ "geen data",
    /* STATUS_CHARGING      */ "opladen",
    /* STATUS_USB           */ "USB",
    /* SLEEP_ZZZ            */ "Zzz...",
    /* TIME_HOUR            */ "uur",
    /* TIME_MINUTE          */ "minuut",
    /* TIME_DAY             */ "dag",
    /* TIME_MONTH           */ "maand",
    /* TIME_YEAR            */ "jaar",
  }
};
