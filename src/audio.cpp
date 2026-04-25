#include "audio.h"

#include <Arduino.h>
#include <ESP_I2S.h>
#include <Wire.h>
#include <math.h>

extern "C" {
#include "es8311.h"
}

#include "pin_config.h"
#include "scream_pcm.h"
#include "settings.h"

namespace {

constexpr uint32_t SAMPLE_RATE = 16000;

I2SClass        i2s;
TaskHandle_t    scream_task     = nullptr;
volatile bool   scream_pending  = false;
volatile bool   audio_ready     = false;

// ES8311 handle blijft bewaard na init zodat we later (bijv. vanuit het
// volume-scherm) nog register-writes kunnen doen.
es8311_handle_t g_es_handle = nullptr;

esp_err_t es8311_codec_init_internal(uint8_t initial_volume) {
  g_es_handle = es8311_create(0, ES8311_ADDRRES_0);
  if (!g_es_handle) return ESP_FAIL;

  const es8311_clock_config_t clk = {
      .mclk_inverted      = false,
      .sclk_inverted      = false,
      .mclk_from_mclk_pin = true,
      .mclk_frequency     = SAMPLE_RATE * 256,
      .sample_frequency   = SAMPLE_RATE,
  };
  if (es8311_init(g_es_handle, &clk, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16) != ESP_OK) return ESP_FAIL;
  if (es8311_sample_frequency_config(g_es_handle, clk.mclk_frequency, clk.sample_frequency) != ESP_OK) return ESP_FAIL;
  if (es8311_voice_volume_set(g_es_handle, initial_volume, nullptr) != ESP_OK) return ESP_FAIL;

  // Dit bord heeft een aparte ES7210 ADC voor het mic-array (zie schematic);
  // de ES8311-ADC mag de gedeelde I2S-SDOUT-lijn niet overnemen. Mic-path hier
  // hard uitschakelen en SDPOUT muten zodat de ES8311 tri-state naar de
  // SDOUT-bus, wat de ES7210 de lijn laat drijven.
  auto wr8311 = [](uint8_t reg, uint8_t val) {
    Wire.beginTransmission(0x18); Wire.write(reg); Wire.write(val); Wire.endTransmission();
  };
  wr8311(0x14, 0x00);    // geen mic-pad geselecteerd (ADC input off)
  wr8311(0x0E, 0xFF);    // power down analog PGA + ADC modulator
  wr8311(0x0A, 0x4C);    // SDPOUT bit6 = 1 (mute) + 16-bit I2S behoud

  return ESP_OK;
}

// Minimale ES7210 init voor dit bord: slave-mode, 16 kHz, 16-bit, MIC1+MIC2,
// ~30 dB PGA. Gebaseerd op esp-adf `components/esp_codec_dev/device/es7210`.
// I2C-adres 0x40 (AD0/AD1 floating -> low, zie schematic U8).
esp_err_t es7210_init() {
  constexpr uint8_t ADDR = 0x40;
  auto wr = [&](uint8_t reg, uint8_t val) -> bool {
    Wire.beginTransmission(ADDR);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
  };
  // I2C-aanwezigheids-check (reg 0x7D = chip id op de meeste revisies).
  Wire.beginTransmission(ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("[es7210] niet gevonden op 0x40");
    return ESP_FAIL;
  }
  bool ok = true;
  // Soft reset + clocks off
  ok &= wr(0x00, 0xFF);
  ok &= wr(0x00, 0x32);
  // Clocks off mask tijdens init (MIC3/4 + bit0 uit, MIC1/2 later aan)
  ok &= wr(0x01, 0x3F);
  // Mainclk / fs / osr
  ok &= wr(0x02, 0xC1);   // mclk div + clear state
  ok &= wr(0x07, 0x20);   // OSR 32
  // Slave-mode (bit0 = 0)
  ok &= wr(0x08, 0x00);
  // Analog power + mic bias
  ok &= wr(0x40, 0x43);   // vdda=3.3V, VMID 5kΩ start
  ok &= wr(0x41, 0x70);   // MIC12 bias 2.87V
  ok &= wr(0x42, 0x70);   // MIC34 bias 2.87V (ongebruikt maar harmless)
  // Mic1+Mic2 kanaal: PGA aan (bit4), gain 30 dB (0x0A, per es7210_gain_value_t)
  ok &= wr(0x43, 0x1A);   // MIC1: enable PGA + 30 dB
  ok &= wr(0x44, 0x1A);   // MIC2: enable PGA + 30 dB
  ok &= wr(0x45, 0x00);   // MIC3: off
  ok &= wr(0x46, 0x00);   // MIC4: off
  // Per-kanaal power: 0x08 = aan met default bias
  ok &= wr(0x47, 0x08);
  ok &= wr(0x48, 0x08);
  ok &= wr(0x49, 0xFF);   // MIC3 power off
  ok &= wr(0x4A, 0xFF);   // MIC4 power off
  ok &= wr(0x4B, 0x00);   // MIC12 bias+ADC+PGA aan
  ok &= wr(0x4C, 0xFF);   // MIC34 off
  // SDP format: I2S, 16-bit (bits[7:5]=011, bits[1:0]=00 -> 0x60)
  ok &= wr(0x11, 0x60);
  // Clocks aan voor MIC1+2 (mask 0x0B gewist vanuit 0x3F)
  ok &= wr(0x01, 0x34);
  // Power-up volgorde
  ok &= wr(0x06, 0x00);   // exit power-down
  ok &= wr(0x40, 0x43);   // herbevestig analog
  ok &= wr(0x00, 0x71);   // reset-state release
  ok &= wr(0x00, 0x41);   // run
  if (!ok) {
    Serial.println("[es7210] I2C-write faalde ergens in de init");
    return ESP_FAIL;
  }
  Serial.println("[es7210] init OK (MIC1+MIC2, 16kHz, 16-bit, 30dB)");
  return ESP_OK;
}

// Scream is een ingebed PCM-sample (16-bit signed LE, 16kHz, stereo),
// gegenereerd uit assets/scream.mp3 via raw_to_header.py.

// Mic-task: leest continu I2S-samples uit de codec en detecteert claps.
// De I2S van Arduino-ESP32 werkt full-duplex -- tegelijk met de scream-task
// die samples uitstuurt kunnen we ook mic-data ontvangen.
void micTask(void* /*arg*/) {
  // Wacht tot de scream-task het I2S + codec heeft geinit.
  while (!audio_ready) { vTaskDelay(pdMS_TO_TICKS(50)); }
  Serial.println("[mic] start clap-detection task");

  constexpr size_t FRAMES         = 256;    // stereo frames per read
  constexpr uint32_t ECHO_COOLDOWN_MS = 500; // na scream nog even mic-events negeren
  int16_t buf[FRAMES * 2];
  uint32_t last_clap_ms    = 0;
  bool     scream_was_on   = false;
  uint32_t scream_ended_ms = 0;

  for (;;) {
    const size_t read = i2s.readBytes((char*)buf, sizeof(buf));
    if (read == 0) { vTaskDelay(pdMS_TO_TICKS(2)); continue; }

    const size_t n_samples = read / sizeof(int16_t);
    int32_t peak = 0;
    for (size_t i = 0; i < n_samples; i++) {
      int32_t s = buf[i];
      if (s < 0) s = -s;
      if (s > peak) peak = s;
    }
    audio._last_mic_peak = peak;

    const uint32_t now = millis();

    // Acoustic-feedback-mute: tijdens een scream pikt de mic vrijwel zeker de
    // speaker op en detecteert dat als een clap. We negeren clap-events zolang
    // de scream loopt + ECHO_COOLDOWN_MS daarna voor na-trillen van het signaal.
    const bool scream_on = scream_pending;
    if (scream_was_on && !scream_on) {
      scream_ended_ms = now;
    }
    scream_was_on = scream_on;
    const bool in_echo_window = scream_on || (now - scream_ended_ms) < ECHO_COOLDOWN_MS;

    // Debounce van 800 ms zodat dubbeltikken niet meervoudig triggeren.
    if (!in_echo_window && peak >= audio._clap_threshold && (now - last_clap_ms) > 800) {
      last_clap_ms = now;
      audio._clap_pending = true;
      Serial.printf("[mic] CLAP (peak=%ld)\n", (long)peak);
    }
  }
}

void screamTask(void* /*arg*/) {
  // I2S + ES8311 init binnen de task zelf op core 1 (zoals Waveshare doet).
  i2s.setPins(I2S_BCK, I2S_WS, I2S_DOUT, I2S_DIN, I2S_MCK);
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE,
                 I2S_DATA_BIT_WIDTH_16BIT,
                 I2S_SLOT_MODE_STEREO,
                 I2S_STD_SLOT_BOTH)) {
    Serial.println("[audio] I2S begin faalde in task");
    vTaskDelete(nullptr);
    return;
  }
  // Ook RX-pad configureren zodat we mic-samples kunnen inlezen via readBytes.
  if (!i2s.configureRX(SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT,
                       I2S_SLOT_MODE_STEREO)) {
    Serial.println("[audio] I2S RX config faalde");
  }
  Wire.begin(IIC_SDA, IIC_SCL);
  if (es8311_codec_init_internal(audio.getVolume()) != ESP_OK) {
    Serial.println("[audio] ES8311 init faalde in task");
    vTaskDelete(nullptr);
    return;
  }
  // ES7210 = aparte ADC voor het on-board mic-array (zie schematic). Faalt deze
  // init, dan werkt clap-detectie niet, maar speaker-output via ES8311 blijft
  // gewoon doordraaien; dus geen task-abort.
  if (es7210_init() != ESP_OK) {
    Serial.println("[audio] ES7210 init faalde -- mic-input blijft stil");
  }
  audio_ready = true;
  Serial.println("[audio] task ready");

  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (!audio_ready) { scream_pending = false; continue; }

    // Speel het embedded PCM-sample in een grote write. i2s.write blokkeert
    // tot de buffer klaar is, dus deze task zit vast totdat de scream voorbij
    // is -- prima, andere cores blijven renderen.
    i2s.write((uint8_t*)scream_pcm, scream_pcm_len);

    scream_pending = false;
  }
}

}  // namespace

Audio audio;

bool Audio::begin() {
  // PA_EN staat al HIGH vanaf main.cpp setup() (voor Wire.begin).
  // I2S + ES8311 init doen we binnen de scream-task op core 1 -- dat was
  // de werkende opzet uit de Waveshare-demo.
  xTaskCreatePinnedToCore(screamTask, "scream", 4096, nullptr,
                          1, &scream_task, 1);

  // Mic-task draait los op core 1 en leest continu samples voor clap-detectie.
  // Wacht tot scream-task het I2S heeft geinit voordat hij read() probeert.
  xTaskCreatePinnedToCore(micTask, "mic", 4096, nullptr,
                          1, nullptr, 1);

  _ready = true;
  Serial.println("[audio] scream+mic tasks gestart op core 1");
  return true;
}

void Audio::playScream() {
  if (!_ready || scream_pending || !scream_task) return;
  scream_pending = true;
  xTaskNotifyGive(scream_task);
}

bool Audio::clapPending() { return _clap_pending; }
void Audio::consumeClap()  { _clap_pending = false; }

void Audio::setVolume(uint8_t v) {
  if (v > 100) v = 100;
  if (v == _volume) return;
  _volume = v;
  if (g_es_handle) {
    // I2C-write; Wire heeft intern een mutex, dus vanuit main core naar
    // codec is veilig ook als de scream-task op core 1 draait.
    es8311_voice_volume_set(g_es_handle, v, nullptr);
  }
  settings.setVolume(v);   // persist (debounced)
}

