// Lumo - hoofdloop met emotie + menu-UI.
// Mode-switching:
//   - Swipe links in EYES -> MENU (4 knoppen)
//   - Tap op knop in MENU -> BATTERY / VOLUME / COLOR / TIME (placeholders)
//   - Tap of swipe rechts in sub-scherm -> terug naar MENU
//   - Swipe rechts in MENU -> terug naar EYES

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <string.h>

#include <esp_sleep.h>
#include <sys/time.h>
#include <time.h>

#include "audio.h"
#include "clock_rtc.h"
#include "emotion.h"
#include "eye_engine.h"
#include "i18n.h"
#include "imu.h"
#include "pin_config.h"
#include "power.h"
#include "settings.h"
#include "touch.h"

// Display-rotatie leeft in settings.orientation() (NVS-persistent, default 1)
// en wordt via applyOrientation() op zowel canvas als touch toegepast.
static constexpr uint32_t SCARED_DURATION_MS       = 3500;
static constexpr uint32_t HAPPY_DURATION_MS     = 2500;
static constexpr uint32_t SURPRISED_DURATION_MS = 2000;
static constexpr uint32_t ANGRY_DURATION_MS     = 3000;
static constexpr uint32_t LOVE_DURATION_MS      = 2000;

static constexpr uint8_t  BATTERY_LOW_PERCENT   = 20;

// Deep-sleep: na N seconden zonder interactie en zonder USB-voeding valt
// Lumo in diepe slaap. Wakker worden via TP_INT (touch).
static constexpr uint32_t SLEEP_TIMEOUT_MS      = 5 * 60 * 1000;   // 5 min
static uint32_t g_last_interaction_ms = 0;

// Tap-spam detectie: 4 interacties binnen 1.5 sec = boos.
static constexpr uint8_t  TAP_SPAM_THRESHOLD    = 4;
static constexpr uint32_t TAP_SPAM_WINDOW_MS    = 1500;

static Arduino_DataBus* bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);

static Arduino_CO5300* panel = new Arduino_CO5300(
    bus, LCD_RESET, 0, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);

static Arduino_Canvas* gfx = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, panel);

static EyeEngine         eyes(gfx);
static EmotionController emotions(eyes);
static Imu               imu;

// --- App mode -------------------------------------------------------------

enum class AppMode : uint8_t {
  EYES,
  MENU,
  BATTERY,
  VOLUME,
  COLOR,
  SETTINGS,
  TIME,
  LANGUAGE,
  ORIENTATION,
};

static AppMode g_mode = AppMode::EYES;

// --- Menu layout ----------------------------------------------------------

enum class Icon : uint8_t {
  BATTERY, VOLUME, COLOR, TIME, SETTINGS, LANGUAGE, ORIENTATION
};

struct MenuButton {
  int16_t  cx, cy;         // centrum van de hele knop (icon + label)
  int16_t  w,  h;          // hitbox voor tap-detectie
  uint16_t accent_color;   // kleur van het icoon
  Icon     icon;
  i18n::T  label_key;      // vertaling-key voor het label
  AppMode  target;
};

// 4 knoppen in de hoeken van het ronde scherm.
// Inscribed circle heeft radius 233; corner-knoppen op radius ~160 van midden
// liggen comfortabel binnen de zichtbare ronde area.
static const MenuButton MENU_BUTTONS[4] = {
  { 120, 130, 130, 110, 0x07E0, Icon::BATTERY,  i18n::T::MENU_BATTERY,  AppMode::BATTERY  },
  { 346, 130, 130, 110, 0x07FF, Icon::VOLUME,   i18n::T::MENU_VOLUME,   AppMode::VOLUME   },
  { 120, 330, 130, 110, 0xFD20, Icon::COLOR,    i18n::T::MENU_COLOR,    AppMode::COLOR    },
  { 346, 330, 130, 110, 0xC618, Icon::SETTINGS, i18n::T::MENU_SETTINGS, AppMode::SETTINGS },
};

// Instellingen-submenu: 3 tegels op een horizontale rij.
static const MenuButton SETTINGS_BUTTONS[3] = {
  {  93, 233, 130, 180, 0xFFE0, Icon::TIME,        i18n::T::MENU_TIME,        AppMode::TIME        },
  { 233, 233, 130, 180, 0x07FF, Icon::LANGUAGE,    i18n::T::MENU_LANGUAGE,    AppMode::LANGUAGE    },
  { 373, 233, 130, 180, 0xFD20, Icon::ORIENTATION, i18n::T::MENU_ORIENTATION, AppMode::ORIENTATION },
};

static bool pointInButton(const TouchPoint& p, const MenuButton& b) {
  if (!p.valid) return false;
  const int16_t hx = b.w / 2;
  const int16_t hy = b.h / 2;
  return p.x >= b.cx - hx && p.x <= b.cx + hx &&
         p.y >= b.cy - hy && p.y <= b.cy + hy;
}

// --- Emotion timeouts -----------------------------------------------------

static uint32_t g_emotion_until = 0;

// Forward declaration (definitie staat verderop in het bestand).
static void drawCenteredText(const char* text, int16_t cy, uint8_t text_size, uint16_t color);

static void triggerEmotion(Emotion e, uint32_t duration_ms, float rate) {
  emotions.setEmotion(e, rate);
  g_emotion_until = millis() + duration_ms;
  g_last_interaction_ms = millis();   // elke emotie-trigger telt als activiteit
}

static void noteInteraction() { g_last_interaction_ms = millis(); }

static void goToDeepSleep() {
  Serial.println("[sleep] naar diepe slaap; wake op touch (GPIO11)");

  // Korte "Zzz" feedback.
  gfx->fillScreen(0x0000);
  drawCenteredText(i18n::t(i18n::T::SLEEP_ZZZ), LCD_HEIGHT / 2, 6, 0x4208);
  gfx->flush();
  delay(1200);

  // Scherm en speaker-amp uit.
  panel->displayOff();
  digitalWrite(PA_EN, LOW);

  // Flush eventuele nog-te-persisten settings naar NVS voor we slapen.
  settings.tick(0xFFFFFFFF);   // forceer direct schrijven

  // Wake op TP_INT (active-low bij aanraking).
  esp_sleep_enable_ext0_wakeup((gpio_num_t)TP_INT, 0);
  esp_deep_sleep_start();
  // komt hier nooit terug -- wake = reboot via setup()
}

// --- Render helpers -------------------------------------------------------

static void drawCenteredText(const char* text, int16_t cy, uint8_t text_size,
                             uint16_t color) {
  // Arduino_GFX default font: 6x8 px per char at size 1.
  const int16_t char_w = 6 * text_size;
  const int16_t char_h = 8 * text_size;
  const int16_t text_w = (int16_t)strlen(text) * char_w;
  gfx->setTextSize(text_size);
  gfx->setTextColor(color);
  gfx->setCursor(LCD_WIDTH / 2 - text_w / 2, cy - char_h / 2);
  gfx->print(text);
}

static void drawBatteryIcon(int16_t cx, int16_t cy, uint16_t color) {
  const int16_t bw = 56, bh = 28;
  // Omtrek (witte rand)
  gfx->drawRoundRect(cx - bw / 2, cy - bh / 2, bw, bh, 4, 0xFFFF);
  gfx->drawRoundRect(cx - bw / 2 + 1, cy - bh / 2 + 1, bw - 2, bh - 2, 3, 0xFFFF);
  // Pluscap rechts
  gfx->fillRect(cx + bw / 2, cy - 6, 4, 12, 0xFFFF);
  // Vulling in de accent-kleur ~75% vol
  const int16_t fill_w = bw - 10;
  gfx->fillRoundRect(cx - bw / 2 + 4, cy - bh / 2 + 4, fill_w, bh - 8, 2, color);
}

static void drawVolumeIcon(int16_t cx, int16_t cy, uint16_t color) {
  // Modern speaker met afgeronde body + schone trapezium-cone
  // Body (kleiner rechterblokje achter de cone)
  gfx->fillRoundRect(cx - 22, cy - 5, 8, 10, 2, color);
  // Cone als trapezium, linker-smaller, rechts breder
  gfx->fillTriangle(cx - 14, cy - 5, cx + 3, cy - 15, cx + 3, cy + 15, color);
  gfx->fillTriangle(cx - 14, cy - 5, cx - 14, cy + 5, cx + 3, cy + 15, color);

  // Drie geluidsgolf-chevrons met oplopende grootte (modern "volume"-look).
  gfx->fillTriangle(cx + 7,  cy - 5,  cx + 10, cy, cx + 7,  cy + 5,  color);
  gfx->fillTriangle(cx + 13, cy - 9,  cx + 17, cy, cx + 13, cy + 9,  color);
  gfx->fillTriangle(cx + 19, cy - 13, cx + 24, cy, cx + 19, cy + 13, color);
}

static void drawColorIcon(int16_t cx, int16_t cy, uint16_t /*unused*/) {
  // Drie overlappende cirkels in R, G, B -- klassiek RGB-wiel.
  const int16_t r = 14;
  gfx->fillCircle(cx,      cy - 8, r, 0xF800); // R boven
  gfx->fillCircle(cx - 12, cy + 6, r, 0x07E0); // G linksonder
  gfx->fillCircle(cx + 12, cy + 6, r, 0x001F); // B rechtsonder
}

static void drawClockIcon(int16_t cx, int16_t cy, uint16_t color) {
  const int16_t r = 22;
  gfx->drawCircle(cx, cy, r,     color);
  gfx->drawCircle(cx, cy, r - 1, color);
  // Klokuurpunten (12 / 3 / 6 / 9)
  gfx->fillRect(cx - 1, cy - r + 1,  2, 4, color);
  gfx->fillRect(cx - 1, cy + r - 5,  2, 4, color);
  gfx->fillRect(cx - r + 1, cy - 1,  4, 2, color);
  gfx->fillRect(cx + r - 5, cy - 1,  4, 2, color);
  // Wijzers (ca 10:10)
  gfx->drawLine(cx, cy, cx - 9, cy - 6, color);  // min
  gfx->drawLine(cx, cy, cx, cy - 10, color);     // uur
}

static void drawSettingsIcon(int16_t cx, int16_t cy, uint16_t color) {
  // Tandwiel-achtig: body-cirkel met 8 bolletjes als tanden, gat in het midden.
  constexpr float R_tooth = 22.0f;
  for (int i = 0; i < 8; i++) {
    const float angle = i * (PI / 4.0f);
    const int16_t tx = cx + (int16_t)lroundf(cosf(angle) * R_tooth);
    const int16_t ty = cy + (int16_t)lroundf(sinf(angle) * R_tooth);
    gfx->fillCircle(tx, ty, 4, color);
  }
  gfx->fillCircle(cx, cy, 16, color);
  gfx->fillCircle(cx, cy,  6, 0x0000);
}

static void drawLanguageIcon(int16_t cx, int16_t cy, uint16_t color) {
  // Globe: dubbele cirkel + meridiaan/equator + twee breedtegraden.
  const int16_t r = 22;
  gfx->drawCircle(cx, cy, r,     color);
  gfx->drawCircle(cx, cy, r - 1, color);
  gfx->drawLine(cx, cy - r, cx, cy + r, color);
  gfx->drawLine(cx - r, cy, cx + r, cy, color);
  gfx->drawLine(cx - r + 5, cy - 10, cx + r - 5, cy - 10, color);
  gfx->drawLine(cx - r + 5, cy + 10, cx + r - 5, cy + 10, color);
}

static void drawOrientationIcon(int16_t cx, int16_t cy, uint16_t color) {
  // Stilistisch "scherm" (staand rechthoekje met puntje bovenaan) + een
  // gebogen pijlspoor eromheen dat rotatie suggereert.
  gfx->drawRoundRect(cx - 13, cy - 18, 26, 36, 4, color);
  gfx->drawRoundRect(cx - 12, cy - 17, 24, 34, 3, color);
  gfx->fillCircle(cx, cy - 12, 2, color);

  // Kwart-arc rechtsonder (= rotatie-indicator) als rij kleine puntjes.
  for (int deg = -20; deg <= 80; deg += 10) {
    const float a = deg * PI / 180.0f;
    const int16_t x = cx + (int16_t)lroundf(cosf(a) * 26.0f);
    const int16_t y = cy + (int16_t)lroundf(sinf(a) * 26.0f);
    gfx->fillCircle(x, y, 2, color);
  }
  // Pijlpunt aan het einde van de arc (onder-rechts).
  gfx->fillTriangle(cx + 5, cy + 26, cx + 12, cy + 21, cx + 12, cy + 30, color);
}

static void drawIcon(Icon icon, int16_t cx, int16_t cy, uint16_t color) {
  switch (icon) {
    case Icon::BATTERY:     drawBatteryIcon(cx, cy, color);     break;
    case Icon::VOLUME:      drawVolumeIcon(cx, cy, color);      break;
    case Icon::COLOR:       drawColorIcon(cx, cy, color);       break;
    case Icon::TIME:        drawClockIcon(cx, cy, color);       break;
    case Icon::SETTINGS:    drawSettingsIcon(cx, cy, color);    break;
    case Icon::LANGUAGE:    drawLanguageIcon(cx, cy, color);    break;
    case Icon::ORIENTATION: drawOrientationIcon(cx, cy, color); break;
  }
}

static void drawButtonLabel(const char* text, int16_t cx, int16_t cy) {
  const int16_t char_w = 12;   // size 2
  const int16_t text_w = (int16_t)strlen(text) * char_w;
  gfx->setTextSize(2);
  gfx->setTextColor(0xFFFF);
  gfx->setCursor(cx - text_w / 2, cy);
  gfx->print(text);
}

static void renderMenu() {
  gfx->fillScreen(0x0000);

  // Iedere hoek-knop: icoon bovenin, label eronder.
  for (const auto& b : MENU_BUTTONS) {
    const int16_t icon_cy  = b.cy - 18;
    const int16_t label_cy = b.cy + 28;
    drawIcon(b.icon, b.cx, icon_cy, b.accent_color);
    drawButtonLabel(i18n::t(b.label_key), b.cx, label_cy);
  }

  // Duidelijke terug-hint in het midden-onder.
  gfx->setTextSize(2);
  gfx->setTextColor(0x8410);   // gedempt grijs
  const char* hint = i18n::t(i18n::T::HINT_SWIPE_BACK);
  const int16_t hw = (int16_t)strlen(hint) * 12;
  gfx->setCursor(LCD_WIDTH / 2 - hw / 2, 222);
  gfx->print(hint);
}

static void renderPlaceholder(const char* title) {
  gfx->fillScreen(0x0000);
  drawCenteredText(title, 180, 5, 0xFFFF);
  drawCenteredText("tik om terug", 320, 2, 0x8410);
}

static void renderSettings() {
  gfx->fillScreen(0x0000);
  drawCenteredText(i18n::t(i18n::T::TITLE_SETTINGS), 60, 3, 0xFFFF);

  for (const auto& b : SETTINGS_BUTTONS) {
    const int16_t icon_cy  = b.cy - 18;
    const int16_t label_cy = b.cy + 28;
    drawIcon(b.icon, b.cx, icon_cy, b.accent_color);
    drawButtonLabel(i18n::t(b.label_key), b.cx, label_cy);
  }

  drawCenteredText(i18n::t(i18n::T::HINT_SWIPE_BACK), 400, 2, 0x8410);
}

// --- Language picker -----------------------------------------------------

// Verticale lijst van tegels, één per geregistreerde taal. Iedere tegel toont
// de menselijke naam van de taal (i18n::name). De actieve taal krijgt een
// lichtere achtergrond + accent-rand. Tap selecteert + persisten.
static constexpr int16_t LANG_TILE_W        = 280;
static constexpr int16_t LANG_TILE_H        = 60;
static constexpr int16_t LANG_TILE_CX       = LCD_WIDTH / 2;
static constexpr int16_t LANG_TILE_CY_TOP   = 150;
static constexpr int16_t LANG_TILE_CY_STEP  = 85;

static void renderLanguageScreen() {
  gfx->fillScreen(0x0000);
  drawCenteredText(i18n::t(i18n::T::TITLE_LANGUAGE), 60, 3, 0xFFFF);

  const uint8_t cur_idx = static_cast<uint8_t>(i18n::current());

  for (uint8_t i = 0; i < i18n::LANG_COUNT; i++) {
    const int16_t cy = LANG_TILE_CY_TOP + i * LANG_TILE_CY_STEP;
    const bool    active = (cur_idx == i);

    const uint16_t bg     = active ? 0x2104 : 0x0000;   // donker blauwgrijs
    const uint16_t fg     = active ? 0x07FF : 0xFFFF;   // cyan voor actief
    const uint16_t border = active ? 0x07FF : 0x4208;

    const int16_t x = LANG_TILE_CX - LANG_TILE_W / 2;
    const int16_t y = cy            - LANG_TILE_H / 2;

    gfx->fillRoundRect(x, y, LANG_TILE_W, LANG_TILE_H, 10, bg);
    gfx->drawRoundRect(x, y, LANG_TILE_W, LANG_TILE_H, 10, border);
    drawCenteredText(i18n::name(static_cast<i18n::Lang>(i)), cy, 3, fg);
  }

  drawCenteredText(i18n::t(i18n::T::HINT_SWIPE_BACK), 425, 2, 0x8410);
}

static bool handleLanguageTap(const TouchPoint& p) {
  if (!p.valid) return false;
  for (uint8_t i = 0; i < i18n::LANG_COUNT; i++) {
    const int16_t cy = LANG_TILE_CY_TOP + i * LANG_TILE_CY_STEP;
    const int16_t x0 = LANG_TILE_CX - LANG_TILE_W / 2;
    const int16_t y0 = cy            - LANG_TILE_H / 2;
    if (p.x >= x0 && p.x <= x0 + LANG_TILE_W &&
        p.y >= y0 && p.y <= y0 + LANG_TILE_H) {
      i18n::setLanguage(static_cast<i18n::Lang>(i));
      settings.setLanguage(i);
      Serial.printf("[lang] -> %s\n", i18n::code(static_cast<i18n::Lang>(i)));
      return true;
    }
  }
  return false;
}

// --- Orientation picker --------------------------------------------------

// Past een rotatie (0..3) toe op zowel de canvas als de touch-mapping.
// Touch-coords volgen de canvas zodat hitboxen ook na rotatie kloppen.
static void applyOrientation(uint8_t rot) {
  rot &= 0x03;
  gfx->setRotation(rot);
  touch.setRotation(rot);
}

// 4 tegels in een 2x2 grid, labels 0 / 90 / 180 / 270.
// Actieve rotatie = gemarkeerd met accent-rand en cyan tekst.
static constexpr int16_t ROT_TILE_W  = 140;
static constexpr int16_t ROT_TILE_H  = 100;
static constexpr int16_t ROT_COL_L   = 153;
static constexpr int16_t ROT_COL_R   = 313;
static constexpr int16_t ROT_ROW_T   = 190;
static constexpr int16_t ROT_ROW_B   = 320;

static const char* const ROT_LABELS[4] = { "0", "90", "180", "270" };

static void rotTileCenter(uint8_t rot, int16_t& cx, int16_t& cy) {
  cx = (rot == 0 || rot == 2) ? ROT_COL_L : ROT_COL_R;
  cy = (rot == 0 || rot == 1) ? ROT_ROW_T : ROT_ROW_B;
}

static void renderOrientationScreen() {
  gfx->fillScreen(0x0000);
  drawCenteredText(i18n::t(i18n::T::TITLE_ORIENTATION), 60, 3, 0xFFFF);

  const uint8_t cur = settings.orientation() & 0x03;

  for (uint8_t r = 0; r < 4; r++) {
    int16_t cx, cy;
    rotTileCenter(r, cx, cy);
    const bool active = (r == cur);

    const uint16_t bg     = active ? 0x2104 : 0x0000;
    const uint16_t fg     = active ? 0x07FF : 0xFFFF;
    const uint16_t border = active ? 0x07FF : 0x4208;

    const int16_t x = cx - ROT_TILE_W / 2;
    const int16_t y = cy - ROT_TILE_H / 2;
    gfx->fillRoundRect(x, y, ROT_TILE_W, ROT_TILE_H, 10, bg);
    gfx->drawRoundRect(x, y, ROT_TILE_W, ROT_TILE_H, 10, border);
    drawCenteredText(ROT_LABELS[r], cy, 5, fg);
  }

  drawCenteredText(i18n::t(i18n::T::HINT_SWIPE_BACK), 425, 2, 0x8410);
}

static bool handleOrientationTap(const TouchPoint& p) {
  if (!p.valid) return false;
  for (uint8_t r = 0; r < 4; r++) {
    int16_t cx, cy;
    rotTileCenter(r, cx, cy);
    const int16_t x0 = cx - ROT_TILE_W / 2;
    const int16_t y0 = cy - ROT_TILE_H / 2;
    if (p.x >= x0 && p.x <= x0 + ROT_TILE_W &&
        p.y >= y0 && p.y <= y0 + ROT_TILE_H) {
      settings.setOrientation(r);
      applyOrientation(r);
      Serial.printf("[orientation] -> %u\n", r);
      return true;
    }
  }
  return false;
}

// --- Battery screen -------------------------------------------------------

static void renderBattery() {
  gfx->fillScreen(0x0000);

  const PowerStatus s = power_mgr.read();

  drawCenteredText(i18n::t(i18n::T::TITLE_BATTERY), 75, 3, 0xFFFF);

  // Grote batterij-icoon + vulling.
  const int16_t bw = 220, bh = 110;
  const int16_t bx = LCD_WIDTH / 2 - bw / 2;
  const int16_t by = 150;
  gfx->drawRoundRect(bx, by, bw, bh, 10, 0xFFFF);
  gfx->drawRoundRect(bx + 1, by + 1, bw - 2, bh - 2, 9, 0xFFFF);
  gfx->fillRect(bx + bw, by + bh / 2 - 14, 8, 28, 0xFFFF);

  if (s.valid) {
    // Kleur op basis van niveau: rood <20%, geel <50%, groen anders.
    uint16_t fill_color = 0x07E0;
    if (s.percent < 20) fill_color = 0xF800;
    else if (s.percent < 50) fill_color = 0xFFE0;
    const int16_t inner_w = bw - 16;
    const int16_t fill_w  = (int16_t)((uint32_t)inner_w * s.percent / 100);
    if (fill_w > 0) {
      gfx->fillRoundRect(bx + 8, by + 8, fill_w, bh - 16, 6, fill_color);
    }

    // Percentage in het midden van de batterij
    char buf[8];
    snprintf(buf, sizeof(buf), "%u%%", s.percent);
    drawCenteredText(buf, by + bh / 2, 4, 0xFFFF);

    // Voltage en charging status onderin
    char line[48];
    if (s.is_charging) {
      snprintf(line, sizeof(line), "%.2fV (%s)",
               s.voltage_mv / 1000.0f, i18n::t(i18n::T::STATUS_CHARGING));
    } else if (s.vbus_present) {
      snprintf(line, sizeof(line), "%.2fV (%s)",
               s.voltage_mv / 1000.0f, i18n::t(i18n::T::STATUS_USB));
    } else {
      snprintf(line, sizeof(line), "%.2fV", s.voltage_mv / 1000.0f);
    }
    drawCenteredText(line, 300, 2, 0xC618);
  } else {
    drawCenteredText(i18n::t(i18n::T::STATUS_NO_DATA), by + bh / 2, 3, 0xF800);
  }

  drawCenteredText(i18n::t(i18n::T::HINT_SWIPE_BACK), 370, 2, 0x8410);
}

// --- Volume screen --------------------------------------------------------

static constexpr int16_t VOL_BAR_X = 45;
static constexpr int16_t VOL_BAR_Y = 260;
static constexpr int16_t VOL_BAR_W = LCD_WIDTH - 2 * VOL_BAR_X;
static constexpr int16_t VOL_BAR_H = 32;
static constexpr int16_t VOL_KNOB_R = 22;

static void renderVolumeScreen() {
  gfx->fillScreen(0x0000);

  drawCenteredText(i18n::t(i18n::T::TITLE_VOLUME), 55, 3, 0xFFFF);

  const uint8_t v = audio.getVolume();

  // Grote percentage-tekst in het midden.
  char buf[8];
  snprintf(buf, sizeof(buf), "%u%%", v);
  drawCenteredText(buf, 160, 6, 0xFFE0);

  // Slider achtergrond (donker grijs, afgerond).
  gfx->fillRoundRect(VOL_BAR_X, VOL_BAR_Y, VOL_BAR_W, VOL_BAR_H, 12, 0x2104);

  // Gevulde deel (fel groen).
  const int16_t fill_w = (int16_t)((int32_t)VOL_BAR_W * v / 100);
  if (fill_w > VOL_BAR_H) {
    gfx->fillRoundRect(VOL_BAR_X, VOL_BAR_Y, fill_w, VOL_BAR_H, 12, 0x07E0);
  } else if (fill_w > 0) {
    gfx->fillRect(VOL_BAR_X, VOL_BAR_Y, fill_w, VOL_BAR_H, 0x07E0);
  }

  // Knop: witte cirkel op de huidige positie.
  const int16_t knob_x = VOL_BAR_X + fill_w;
  const int16_t knob_y = VOL_BAR_Y + VOL_BAR_H / 2;
  gfx->fillCircle(knob_x, knob_y, VOL_KNOB_R, 0xFFFF);
  gfx->drawCircle(knob_x, knob_y, VOL_KNOB_R, 0x4208);

  drawCenteredText(i18n::t(i18n::T::HINT_SWIPE_BACK), 410, 2, 0x8410);
}

// Live drag op de slider: zolang de vinger binnen een genereuze horizontale
// band rond de slider zit, updaten we het volume.
static bool handleVolumeDrag() {
  if (!touch.isTouched()) return false;
  const int16_t ty = touch.touchY();
  if (ty < VOL_BAR_Y - 40 || ty > VOL_BAR_Y + VOL_BAR_H + 40) return false;

  int16_t tx = touch.touchX();
  if (tx < VOL_BAR_X)              tx = VOL_BAR_X;
  if (tx > VOL_BAR_X + VOL_BAR_W)  tx = VOL_BAR_X + VOL_BAR_W;

  const uint8_t v = (uint8_t)(((int32_t)(tx - VOL_BAR_X) * 100) / VOL_BAR_W);
  audio.setVolume(v);
  return true;
}

// --- Color picker ---------------------------------------------------------

// HSV->RGB565 (v1 vast op s=1, v=1 - volle verzadigde hues).
static uint16_t hsvToRgb565(float h) {
  while (h < 0)   h += 360;
  while (h >= 360) h -= 360;
  const float c  = 1.0f;
  const float x  = 1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f);
  float rf = 0, gf = 0, bf = 0;
  if      (h <  60) { rf = c; gf = x; }
  else if (h < 120) { rf = x; gf = c; }
  else if (h < 180) {         gf = c; bf = x; }
  else if (h < 240) {         gf = x; bf = c; }
  else if (h < 300) { rf = x;         bf = c; }
  else              { rf = c;         bf = x; }
  const uint8_t r5 = (uint8_t)(rf * 31);
  const uint8_t g6 = (uint8_t)(gf * 63);
  const uint8_t b5 = (uint8_t)(bf * 31);
  return (uint16_t)(r5 << 11) | (uint16_t)(g6 << 5) | b5;
}

// Hue-wheel afmetingen.
static constexpr int16_t COLOR_CX      = LCD_WIDTH  / 2;
static constexpr int16_t COLOR_CY      = LCD_HEIGHT / 2;
static constexpr int16_t RING_R_INNER  = 150;
static constexpr int16_t RING_R_OUTER  = 210;
static constexpr int16_t CENTER_R      = 130;   // centrale preview-cirkel
static constexpr int     RING_SEGMENTS = 72;    // 5 graden per wedge

static void renderColorPicker() {
  gfx->fillScreen(0x0000);

  // Hue-ring: 72 wedges van 5 graden. Rood (hue=0) op 12 uur, klok mee.
  for (int i = 0; i < RING_SEGMENTS; i++) {
    const float hue = (float)i * 360.0f / RING_SEGMENTS;
    const float a0  = (hue - 90.0f) * DEG_TO_RAD;
    const float a1  = (hue + (360.0f / RING_SEGMENTS) - 90.0f) * DEG_TO_RAD;

    const int16_t x0o = COLOR_CX + (int16_t)(RING_R_OUTER * cosf(a0));
    const int16_t y0o = COLOR_CY + (int16_t)(RING_R_OUTER * sinf(a0));
    const int16_t x1o = COLOR_CX + (int16_t)(RING_R_OUTER * cosf(a1));
    const int16_t y1o = COLOR_CY + (int16_t)(RING_R_OUTER * sinf(a1));
    const int16_t x0i = COLOR_CX + (int16_t)(RING_R_INNER * cosf(a0));
    const int16_t y0i = COLOR_CY + (int16_t)(RING_R_INNER * sinf(a0));
    const int16_t x1i = COLOR_CX + (int16_t)(RING_R_INNER * cosf(a1));
    const int16_t y1i = COLOR_CY + (int16_t)(RING_R_INNER * sinf(a1));

    const uint16_t col = hsvToRgb565(hue);
    gfx->fillTriangle(x0o, y0o, x1o, y1o, x0i, y0i, col);
    gfx->fillTriangle(x1o, y1o, x1i, y1i, x0i, y0i, col);
  }

  // Preview-cirkel in het midden = huidige gekozen kleur.
  gfx->fillCircle(COLOR_CX, COLOR_CY, CENTER_R, getGlobalEyeColor());

  // Subtiele hint boven en onder de ring.
  drawCenteredText(i18n::t(i18n::T::HINT_DRAG_RING),        30, 2, 0xC618);
  drawCenteredText(i18n::t(i18n::T::HINT_TAP_CENTER_BACK), 440, 2, 0x8410);
}

// Live drag: zolang de vinger op de ring ligt, update kleur per frame.
// True als er daadwerkelijk op de ring werd gedragd (behandeld).
static bool handleColorDrag() {
  if (!touch.isTouched()) return false;
  const int16_t dx = touch.touchX() - COLOR_CX;
  const int16_t dy = touch.touchY() - COLOR_CY;
  const int32_t r2 = (int32_t)dx * dx + (int32_t)dy * dy;
  if (r2 < (int32_t)RING_R_INNER * RING_R_INNER - 800) return false;   // te dicht bij midden
  if (r2 > (int32_t)RING_R_OUTER * RING_R_OUTER + 2000) return false;  // voorbij ring

  float angle = atan2f((float)dy, (float)dx);            // -PI..PI (0 = 3 uur)
  float hue_rad = angle + HALF_PI;                       // 0 op 12 uur
  if (hue_rad < 0)          hue_rad += TWO_PI;
  if (hue_rad >= TWO_PI)    hue_rad -= TWO_PI;
  const float hue = hue_rad * 180.0f / PI;
  const uint16_t c = hsvToRgb565(hue);
  setGlobalEyeColor(c);
  settings.setEyeColor(c);   // persist (debounced)
  emotions.refresh(1.0f);
  return true;
}

// --- Input routing --------------------------------------------------------

// Houdt bij hoeveel opeenvolgende taps we in het korte venster hebben gezien.
// Retourneert true als we over de drempel zijn (dan wordt Lumo boos).
static bool registerTapAndCheckSpam(uint8_t tap_weight) {
  static uint32_t last_tap_ms = 0;
  static uint8_t  streak      = 0;
  const uint32_t now = millis();
  if (now - last_tap_ms < TAP_SPAM_WINDOW_MS) {
    streak += tap_weight;
  } else {
    streak = tap_weight;
  }
  last_tap_ms = now;
  if (streak >= TAP_SPAM_THRESHOLD) {
    streak = 0;   // reset zodat opnieuw pesten opnieuw moet escaleren
    return true;
  }
  return false;
}

static void handleEyesInput(TouchEvent te) {
  // Tijdens ANGRY mokt Lumo en negeert hij alle touch/swipe input.
  // IMU pickup wordt apart afgehandeld (buiten deze functie) en blijft werken.
  if (emotions.current() == Emotion::ANGRY) return;

  if (te == TouchEvent::SWIPE_LEFT) {
    g_mode = AppMode::MENU;
    return;
  }

  // SINGLE telt als 1 tap, DOUBLE als 2 (het waren immers twee fysieke tikken).
  if (te == TouchEvent::SINGLE_TAP || te == TouchEvent::DOUBLE_TAP) {
    const uint8_t weight = (te == TouchEvent::DOUBLE_TAP) ? 2 : 1;
    if (registerTapAndCheckSpam(weight)) {
      Serial.println(">>> ANGRY (te veel tikken)");
      triggerEmotion(Emotion::ANGRY, ANGRY_DURATION_MS, 1.0f);
      eyes.shake(4, 2500);   // subtiele tril van irritatie
      return;
    }
  }

  if (te == TouchEvent::SINGLE_TAP) {
    Serial.println(">>> HAPPY (tap)");
    triggerEmotion(Emotion::HAPPY, HAPPY_DURATION_MS, 0.12f);
  } else if (te == TouchEvent::DOUBLE_TAP) {
    Serial.println(">>> SURPRISED (double tap)");
    triggerEmotion(Emotion::SURPRISED, SURPRISED_DURATION_MS, 1.0f);
  }
}

static void handleMenuInput(TouchEvent te) {
  if (te == TouchEvent::SWIPE_RIGHT) {
    g_mode = AppMode::EYES;
    return;
  }
  if (te == TouchEvent::SINGLE_TAP) {
    const TouchPoint p = touch.lastTapPoint();
    Serial.printf("[menu] tap x=%d y=%d\n", p.x, p.y);
    for (const auto& b : MENU_BUTTONS) {
      if (pointInButton(p, b)) {
        g_mode = b.target;
        Serial.printf("[menu] -> %s\n", i18n::t(b.label_key));
        return;
      }
    }
  }
}

static void handleSettingsMenuInput(TouchEvent te) {
  if (te == TouchEvent::SWIPE_RIGHT) {
    g_mode = AppMode::MENU;
    return;
  }
  if (te == TouchEvent::SINGLE_TAP) {
    const TouchPoint p = touch.lastTapPoint();
    for (const auto& b : SETTINGS_BUTTONS) {
      if (pointInButton(p, b)) {
        g_mode = b.target;
        Serial.printf("[settings] -> %s\n", i18n::t(b.label_key));
        return;
      }
    }
  }
}

// --- Time setter screen ---------------------------------------------------

enum TimeField : uint8_t {
  FIELD_HOUR = 0,
  FIELD_MINUTE,
  FIELD_DAY,
  FIELD_MONTH,
  FIELD_YEAR,
  FIELD_COUNT
};

static const i18n::T TIME_FIELD_KEYS[FIELD_COUNT] = {
  i18n::T::TIME_HOUR,
  i18n::T::TIME_MINUTE,
  i18n::T::TIME_DAY,
  i18n::T::TIME_MONTH,
  i18n::T::TIME_YEAR,
};

static uint8_t g_edit_field = FIELD_HOUR;

static int daysInMonth(int year, int month_1_based) {
  static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month_1_based < 1 || month_1_based > 12) return 31;
  if (month_1_based == 2) {
    const bool leap = ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
    return leap ? 29 : 28;
  }
  return d[month_1_based - 1];
}

static void bumpCurrentField(int delta) {
  time_t now = time(nullptr);
  struct tm tm_v; localtime_r(&now, &tm_v);

  switch (g_edit_field) {
    case FIELD_HOUR:
      tm_v.tm_hour = (tm_v.tm_hour + delta + 24) % 24;
      break;
    case FIELD_MINUTE:
      tm_v.tm_min  = (tm_v.tm_min  + delta + 60) % 60;
      break;
    case FIELD_DAY: {
      const int max_d = daysInMonth(tm_v.tm_year + 1900, tm_v.tm_mon + 1);
      tm_v.tm_mday = ((tm_v.tm_mday - 1 + delta + max_d) % max_d) + 1;
      break;
    }
    case FIELD_MONTH: {
      tm_v.tm_mon = (tm_v.tm_mon + delta + 12) % 12;
      const int max_d = daysInMonth(tm_v.tm_year + 1900, tm_v.tm_mon + 1);
      if (tm_v.tm_mday > max_d) tm_v.tm_mday = max_d;
      break;
    }
    case FIELD_YEAR: {
      int year = tm_v.tm_year + 1900 + delta;
      if (year < 2020) year = 2020;
      if (year > 2099) year = 2099;
      tm_v.tm_year = year - 1900;
      const int max_d = daysInMonth(year, tm_v.tm_mon + 1);
      if (tm_v.tm_mday > max_d) tm_v.tm_mday = max_d;
      break;
    }
    default: break;
  }
  tm_v.tm_sec = 0;
  time_t new_t = mktime(&tm_v);
  struct timeval tv = {new_t, 0};
  settimeofday(&tv, nullptr);
}

struct Rect { int16_t x, y, w, h; };
static constexpr Rect FIELD_LEFT_ARROW  = { 70,  170, 60, 60 };
static constexpr Rect FIELD_RIGHT_ARROW = { 336, 170, 60, 60 };
static constexpr Rect MINUS_BUTTON      = { 70,  320, 90, 90 };
static constexpr Rect PLUS_BUTTON       = { 306, 320, 90, 90 };

static bool pointInRect(const TouchPoint& p, const Rect& r) {
  return p.valid && p.x >= r.x && p.x <= r.x + r.w &&
                    p.y >= r.y && p.y <= r.y + r.h;
}

static void drawArrowTriangle(int16_t cx, int16_t cy, bool left, uint16_t color) {
  const int16_t s = 20;
  if (left) {
    gfx->fillTriangle(cx + s, cy - s, cx + s, cy + s, cx - s, cy, color);
  } else {
    gfx->fillTriangle(cx - s, cy - s, cx - s, cy + s, cx + s, cy, color);
  }
}

static void renderTimeScreen() {
  gfx->fillScreen(0x0000);

  time_t now = time(nullptr);
  struct tm tm_v; localtime_r(&now, &tm_v);

  drawCenteredText(i18n::t(i18n::T::TITLE_TIME), 50, 3, 0xFFFF);

  char line[32];
  snprintf(line, sizeof(line), "%02d:%02d  %02d-%02d-%04d",
           tm_v.tm_hour, tm_v.tm_min,
           tm_v.tm_mday, tm_v.tm_mon + 1, tm_v.tm_year + 1900);
  drawCenteredText(line, 110, 2, 0xC618);

  drawArrowTriangle(100, 200, true,  0xFFFF);
  drawArrowTriangle(366, 200, false, 0xFFFF);
  drawCenteredText(i18n::t(TIME_FIELD_KEYS[g_edit_field]), 200, 3, 0x07FF);

  char val[8];
  int value = 0;
  int width = 2;
  switch (g_edit_field) {
    case FIELD_HOUR:   value = tm_v.tm_hour;        break;
    case FIELD_MINUTE: value = tm_v.tm_min;         break;
    case FIELD_DAY:    value = tm_v.tm_mday;        break;
    case FIELD_MONTH:  value = tm_v.tm_mon + 1;     break;
    case FIELD_YEAR:   value = tm_v.tm_year + 1900; width = 4; break;
    default: break;
  }
  snprintf(val, sizeof(val), "%0*d", width, value);
  drawCenteredText(val, 265, 6, 0xFFFF);

  // Minus-knop (grijs)
  gfx->fillCircle(MINUS_BUTTON.x + MINUS_BUTTON.w/2,
                  MINUS_BUTTON.y + MINUS_BUTTON.h/2, 40, 0x7BEF);
  gfx->fillRect(MINUS_BUTTON.x + 20, MINUS_BUTTON.y + 40,
                MINUS_BUTTON.w - 40, 10, 0xFFFF);

  // Plus-knop (groen)
  gfx->fillCircle(PLUS_BUTTON.x + PLUS_BUTTON.w/2,
                  PLUS_BUTTON.y + PLUS_BUTTON.h/2, 40, 0x07E0);
  gfx->fillRect(PLUS_BUTTON.x + 20, PLUS_BUTTON.y + 40,
                PLUS_BUTTON.w - 40, 10, 0xFFFF);
  gfx->fillRect(PLUS_BUTTON.x + 40, PLUS_BUTTON.y + 20,
                10, PLUS_BUTTON.h - 40, 0xFFFF);

  drawCenteredText(i18n::t(i18n::T::HINT_SWIPE_SAVE), 435, 2, 0x8410);
}

static bool handleTimeTap(const TouchPoint& p) {
  if (!p.valid) return false;
  if (pointInRect(p, FIELD_LEFT_ARROW)) {
    g_edit_field = (uint8_t)((g_edit_field + FIELD_COUNT - 1) % FIELD_COUNT);
    return true;
  }
  if (pointInRect(p, FIELD_RIGHT_ARROW)) {
    g_edit_field = (uint8_t)((g_edit_field + 1) % FIELD_COUNT);
    return true;
  }
  if (pointInRect(p, MINUS_BUTTON)) { bumpCurrentField(-1); return true; }
  if (pointInRect(p, PLUS_BUTTON))  { bumpCurrentField(+1); return true; }
  return false;
}

// --- Input routing --------------------------------------------------------

static void handleSubscreenInput(TouchEvent te) {
  // Swipe-rechts gaat altijd een niveau omhoog. TIME + LANGUAGE zitten onder
  // SETTINGS; andere subschermen hangen direct onder het hoofdmenu.
  // Bij TIME schrijven we de nieuwe tijd ook nog naar de RTC.
  if (te == TouchEvent::SWIPE_RIGHT) {
    if (g_mode == AppMode::TIME) clock_rtc.saveSystemTimeToRtc();
    const bool under_settings = (g_mode == AppMode::TIME ||
                                 g_mode == AppMode::LANGUAGE ||
                                 g_mode == AppMode::ORIENTATION);
    g_mode = under_settings ? AppMode::SETTINGS : AppMode::MENU;
    return;
  }

  if (te == TouchEvent::SINGLE_TAP) {
    if (g_mode == AppMode::VOLUME) {
      // In VOLUME: tap = volume direct op aangetikte positie zetten; geen
      // terug-gaan want dat conflicteert met de slider-tap.
      const TouchPoint p = touch.lastTapPoint();
      if (p.valid && p.y >= VOL_BAR_Y - 40 && p.y <= VOL_BAR_Y + VOL_BAR_H + 40) {
        int16_t x = p.x;
        if (x < VOL_BAR_X)              x = VOL_BAR_X;
        if (x > VOL_BAR_X + VOL_BAR_W)  x = VOL_BAR_X + VOL_BAR_W;
        audio.setVolume((uint8_t)(((int32_t)(x - VOL_BAR_X) * 100) / VOL_BAR_W));
        return;
      }
      // tap ergens anders -> terug
      g_mode = AppMode::MENU;
      return;
    }
    if (g_mode == AppMode::COLOR) {
      // In COLOR: tap in het midden = terug; tap op de ring wordt al via
      // drag-poll afgehandeld, dus alleen reageren op midden-tap.
      const TouchPoint p = touch.lastTapPoint();
      const int16_t dx = p.x - COLOR_CX;
      const int16_t dy = p.y - COLOR_CY;
      if ((int32_t)dx * dx + (int32_t)dy * dy <= (int32_t)CENTER_R * CENTER_R) {
        g_mode = AppMode::MENU;
      }
      return;
    }
    if (g_mode == AppMode::TIME) {
      // Alleen terug naar Instellingen als we NIET op een tijd-setter knop klikten.
      if (!handleTimeTap(touch.lastTapPoint())) {
        g_mode = AppMode::SETTINGS;
      }
      return;
    }
    if (g_mode == AppMode::LANGUAGE) {
      // Tap op een taal-tegel = selectie + persist. Tap buiten een tegel
      // gaat een niveau omhoog naar het instellingen-menu.
      if (!handleLanguageTap(touch.lastTapPoint())) {
        g_mode = AppMode::SETTINGS;
      }
      return;
    }
    if (g_mode == AppMode::ORIENTATION) {
      // Tap op een rotatie-tegel = selectie + directe toepassing op canvas
      // en touch-mapping; volgende frame rendert al geroteerd. Tap buiten
      // gaat omhoog.
      if (!handleOrientationTap(touch.lastTapPoint())) {
        g_mode = AppMode::SETTINGS;
      }
      return;
    }
    g_mode = AppMode::MENU;
  }
}

// --- Setup / loop ---------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Lumo boot");

  // Waveshare's demo doet PA_EN HIGH als allereerste, ver voor Wire.begin.
  // We repliceren dat hier exact voor de audio-hardware-test.
  pinMode(PA_EN, OUTPUT);
  digitalWrite(PA_EN, HIGH);

  Wire.begin(IIC_SDA, IIC_SCL);

  // Settings eerst inladen zodat volume/kleur/taal meteen correct zijn.
  settings.begin();
  setGlobalEyeColor(settings.eyeColor());
  i18n::begin(static_cast<i18n::Lang>(settings.language()));

  if (!gfx->begin()) {
    Serial.println("canvas init faalde (PSRAM?)");
    while (true) { delay(1000); }
  }
  gfx->setRotation(settings.orientation());
  panel->setBrightness(180);

  eyes.begin();
  eyes.snapTo(presetFor(Emotion::NEUTRAL));
  emotions.setEmotion(Emotion::NEUTRAL, 0.12f);

  if (!imu.begin())        Serial.println("doorgaan zonder IMU");
  if (!touch.begin())      Serial.println("doorgaan zonder touch");
  else                     touch.setRotation(settings.orientation());
  if (!audio.begin())      Serial.println("doorgaan zonder audio");
  audio.setVolume(settings.volume());   // pas opgeslagen volume toe
  if (!power_mgr.begin())  Serial.println("doorgaan zonder power-info");
  if (!clock_rtc.begin())  Serial.println("doorgaan zonder RTC");

  // Als er na RTC-init nog geen plausibele tijd is, zet een werkbare default.
  {
    time_t t_now = time(nullptr);
    struct tm tm_v; localtime_r(&t_now, &tm_v);
    if (tm_v.tm_year + 1900 < 2020) {
      struct tm init_tm = {};
      init_tm.tm_year = 2026 - 1900;
      init_tm.tm_mon  = 3;        // april (0-based)
      init_tm.tm_mday = 22;
      init_tm.tm_hour = 12;
      time_t t = mktime(&init_tm);
      struct timeval tv = {t, 0};
      settimeofday(&tv, nullptr);
    }
  }

  // Als we uit deep sleep komen (wake op touch), toon kort SURPRISED -- alsof
  // Lumo wakker schrikt van de aanraking.
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("[sleep] wake op touch -> SURPRISED");
    triggerEmotion(Emotion::SURPRISED, SURPRISED_DURATION_MS, 1.0f);
  }

  g_last_interaction_ms = millis();
  Serial.println("klaar");
}

// Houd bij of de batterij laag is, zodat return-to-idle SLEEPY kiest i.p.v.
// NEUTRAL. Wordt door tickPowerEvents bijgehouden.
static bool g_battery_low = false;

// Power-events: USB unplug -> SCARED, USB plug -> LOVE (hartjes), en
// permanent SLEEPY zolang de accu onder BATTERY_LOW_PERCENT zit.
static void tickPowerEvents(uint32_t now) {
  static uint32_t next_poll        = 0;
  static bool     initialized      = false;
  static bool     was_vbus_present = true;

  if (now < next_poll) return;
  next_poll = now + 1500;   // 1.5s interval

  const PowerStatus s = power_mgr.read();
  if (!s.valid) return;

  if (!initialized) {
    was_vbus_present = s.vbus_present;
    initialized = true;
    g_battery_low = (!s.is_charging && s.percent <= BATTERY_LOW_PERCENT);
    return;
  }

  // USB losgekoppeld -> boos (rode gefronste ogen + subtiele tril).
  if (was_vbus_present && !s.vbus_present) {
    Serial.println(">>> ANGRY (USB losgekoppeld)");
    g_mode = AppMode::EYES;
    triggerEmotion(Emotion::ANGRY, ANGRY_DURATION_MS, 1.0f);
    eyes.shake(4, 2500);
  }
  // USB aangesloten -> hart-ogen kort.
  if (!was_vbus_present && s.vbus_present) {
    Serial.println(">>> LOVE (USB aangesloten)");
    g_mode = AppMode::EYES;
    triggerEmotion(Emotion::LOVE, LOVE_DURATION_MS, 0.5f);
  }
  was_vbus_present = s.vbus_present;

  // Batterij-laag: wordt een "sticky" flag die de idle-staat naar SLEEPY trekt.
  const bool now_low = (!s.is_charging && s.percent <= BATTERY_LOW_PERCENT);
  if (now_low != g_battery_low) {
    Serial.printf("[power] battery_low %d -> %d (%u%%)\n",
                  g_battery_low, now_low, s.percent);
    g_battery_low = now_low;
  }
}

// Idle-animatie: soms kijken de ogen random naar links/rechts/boven/onder
// zolang we in EYES-mode zijn en de emotie NEUTRAL is. Geeft leven aan de
// idle-staat, zoals bij een mens.
static void tickIdleLook(uint32_t now) {
  if (g_mode != AppMode::EYES) return;
  if (emotions.current() != Emotion::NEUTRAL) return;

  static uint32_t next_look_change_ms = 0;
  if (now < next_look_change_ms) return;

  // ~40% kans: keer terug naar midden. Anders: random punt.
  const uint32_t roll = esp_random() % 100;
  if (roll < 40) {
    eyes.setLookTarget(0.0f, 0.0f);
  } else {
    // look_x in [-0.7, 0.7], look_y in [-0.3, 0.3] (horizontaal meer dan verticaal).
    const float lx = ((float)((int)(esp_random() % 1401) - 700)) / 1000.0f;
    const float ly = ((float)((int)(esp_random() %  601) - 300)) / 1000.0f;
    eyes.setLookTarget(lx, ly);
  }
  // Mensen knipperen vaak tijdens een saccade -- 60% kans dat look-change
  // samengaat met een blink, zodat beweging en knipperen door elkaar lopen.
  if ((esp_random() % 100) < 60) {
    eyes.blink();
  }
  // Volgend moment: 1.5 - 5 sec.
  next_look_change_ms = now + 1500 + (esp_random() % 3501);
}

void loop() {
  const uint32_t now = millis();

  imu.tick(now);
  touch.tick(now);
  tickPowerEvents(now);
  tickIdleLook(now);
  settings.tick(now);

  // Interactie-activiteit detecteren voor het deep-sleep timeout.
  if (imu.peekEvent() != MotionEvent::NONE || touch.peekEvent() != TouchEvent::NONE ||
      touch.isTouched()) {
    noteInteraction();
  }

  // Deep sleep als er geen USB is, de accu-stand goedwerkend + lang genoeg idle.
  const PowerStatus ps = power_mgr.read();
  const bool on_battery = ps.valid && !ps.vbus_present && !ps.is_charging;
  if (on_battery && g_mode == AppMode::EYES &&
      now - g_last_interaction_ms > SLEEP_TIMEOUT_MS) {
    goToDeepSleep();
  }

  // IMU pickup werkt in elk scherm: een schrikmoment overschrijft alles.
  if (imu.peekEvent() == MotionEvent::PICKED_UP) {
    imu.consumeEvent();
    Serial.println(">>> SCARED (pickup)");
    g_mode = AppMode::EYES;
    triggerEmotion(Emotion::SCARED, SCARED_DURATION_MS, 1.0f);
    eyes.shake(14, 1200);
    audio.playScream();
  }

  // Clap-detectie: iemand klapt naast Lumo -> SURPRISED.
  if (audio.clapPending()) {
    audio.consumeClap();
    // Negeren als Lumo zelf net geluid maakt (anders feedback van speaker).
    // Voor v1: accepteren en laat de debounce in de mic-task het afhandelen.
    Serial.println(">>> SURPRISED (clap)");
    g_mode = AppMode::EYES;
    triggerEmotion(Emotion::SURPRISED, SURPRISED_DURATION_MS, 1.0f);
  }

  // Touch events routeren op basis van huidige mode.
  const TouchEvent te = touch.peekEvent();
  if (te != TouchEvent::NONE) {
    touch.consumeEvent();
    switch (g_mode) {
      case AppMode::EYES:     handleEyesInput(te);         break;
      case AppMode::MENU:     handleMenuInput(te);         break;
      case AppMode::SETTINGS: handleSettingsMenuInput(te); break;
      default:                handleSubscreenInput(te);    break;
    }
  }

  // Live drag in COLOR-mode: hue updaten terwijl vinger over de ring sleept.
  if (g_mode == AppMode::COLOR) {
    handleColorDrag();
  }
  // Live drag in VOLUME-mode: volume updaten tijdens slepen op de slider.
  if (g_mode == AppMode::VOLUME) {
    handleVolumeDrag();
  }

  // Terug naar idle na emotie-duration. Kies SLEEPY als accu leeg, anders
  // NEUTRAL. SLEEPY zelf triggeren we niet opnieuw want dat is al de idle.
  if (g_mode == AppMode::EYES &&
      emotions.current() != Emotion::NEUTRAL &&
      emotions.current() != Emotion::SLEEPY &&
      now >= g_emotion_until) {
    emotions.setEmotion(g_battery_low ? Emotion::SLEEPY : Emotion::NEUTRAL, 0.06f);
  }
  // Accu hersteld / opgeladen: wakker worden uit SLEEPY.
  if (g_mode == AppMode::EYES &&
      emotions.current() == Emotion::SLEEPY && !g_battery_low) {
    emotions.setEmotion(Emotion::NEUTRAL, 0.06f);
  }
  // Accu leeg terwijl we NEUTRAL zijn: val direct in slaap.
  if (g_mode == AppMode::EYES &&
      emotions.current() == Emotion::NEUTRAL && g_battery_low) {
    emotions.setEmotion(Emotion::SLEEPY, 0.06f);
  }

  // Render op basis van mode.
  switch (g_mode) {
    case AppMode::EYES:
      eyes.tick(now);
      eyes.render();
      break;
    case AppMode::MENU:
      renderMenu();
      break;
    case AppMode::BATTERY:    renderBattery();           break;
    case AppMode::VOLUME:     renderVolumeScreen();      break;
    case AppMode::COLOR:      renderColorPicker();       break;
    case AppMode::SETTINGS:   renderSettings();          break;
    case AppMode::TIME:       renderTimeScreen();        break;
    case AppMode::LANGUAGE:   renderLanguageScreen();    break;
    case AppMode::ORIENTATION: renderOrientationScreen(); break;
  }
  gfx->flush();

  static uint32_t last_frame = 0;
  const uint32_t dt = now - last_frame;
  if (dt < 20) { delay(20 - dt); }
  last_frame = millis();
}
