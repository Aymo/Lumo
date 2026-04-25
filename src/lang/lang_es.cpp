// Traducciones al español.
//
// El orden del array de strings DEBE coincidir con el enum i18n::T en i18n.h.
//
// NOTE: Spanish has accented characters (á, é, í, ó, ú, ñ). Arduino_GFX's
// default 5x7 font does not render UTF-8 multibyte sequences cleanly, so most
// words here use an ASCII-safe form ("Bateria" i.p.v. "Batería"). De één
// uitzondering is "año" (year): zonder tilde wordt dat "ano", wat in het
// Spaans een vulgair woord is — daarom staat hier bewust "año" in UTF-8.
// Als accents alsnog verkeerd renderen, is de nette oplossing een custom GFX-
// font. Tot dat moment: gelieve deze strings niet "op te schonen".

#include "../i18n.h"

extern const i18n::LangTable LANG_ES = {
  "ES",
  "Espanol",
  {
    /* MENU_BATTERY         */ "Bateria",
    /* MENU_VOLUME          */ "Volumen",
    /* MENU_COLOR           */ "Color",
    /* MENU_SETTINGS        */ "Ajustes",
    /* MENU_TIME            */ "Hora",
    /* MENU_LANGUAGE        */ "Idioma",
    /* MENU_ORIENTATION     */ "Orientacion",
    /* TITLE_SETTINGS       */ "Ajustes",
    /* TITLE_BATTERY        */ "Bateria",
    /* TITLE_VOLUME         */ "Volumen",
    /* TITLE_TIME           */ "Hora y fecha",
    /* TITLE_LANGUAGE       */ "Idioma",
    /* TITLE_ORIENTATION    */ "Orientacion",
    /* HINT_SWIPE_BACK      */ "desliza -> atras",
    /* HINT_SWIPE_SAVE      */ "desliza -> guardar",
    /* HINT_DRAG_RING       */ "arrastra el anillo",
    /* HINT_TAP_CENTER_BACK */ "toca centro -> atras",
    /* STATUS_NO_DATA       */ "sin datos",
    /* STATUS_CHARGING      */ "cargando",
    /* STATUS_USB           */ "USB",
    /* SLEEP_ZZZ            */ "Zzz...",
    /* TIME_HOUR            */ "hora",
    /* TIME_MINUTE          */ "minuto",
    /* TIME_DAY             */ "dia",
    /* TIME_MONTH           */ "mes",
    /* TIME_YEAR            */ "año",
  }
};
