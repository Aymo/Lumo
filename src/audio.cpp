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
  if (es8311_microphone_config(g_es_handle, false) != ESP_OK) return ESP_FAIL;
  if (es8311_voice_volume_set(g_es_handle, initial_volume, nullptr) != ESP_OK) return ESP_FAIL;
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

  constexpr size_t FRAMES = 256;   // stereo frames per read
  int16_t buf[FRAMES * 2];
  uint32_t last_clap_ms = 0;

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
    // Debounce van 800 ms zodat dubbeltikken niet meervoudig triggeren.
    if (peak >= audio._clap_threshold && (now - last_clap_ms) > 800) {
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

