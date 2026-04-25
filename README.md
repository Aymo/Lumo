# Lumo

**Lightweight Universal Moji OS** — open-source firmware that turns a small ESP32-S3 AMOLED display into an expressive, emotional desk companion.

[![License: GPL v3](https://img.shields.io/badge/License-GPL_v3-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-ESP32--S3-orange.svg)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Hardware](https://img.shields.io/badge/hardware-Waveshare_AMOLED_1.75-green.svg)](https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm)

> ⚡ **[Flash it now in your browser →](https://aymo.github.io/Lumo/)**
> No installation. No toolchain. Just a USB cable and Chrome or Edge.

---

## What is Lumo?

Lumo is a configurable **expressive-eyes firmware** for round AMOLED displays on ESP32-S3 hardware. It's a tiny face engine: animated eyes that blink, look around, react to touch and motion, and express emotions through shape, color, and movement.

Use Lumo to build a desk companion, a face for your robot, an ambient mood display, an interactive art piece — whatever your maker brain comes up with.

Lumo is **GPL-3.0 licensed**. Use it, modify it, share it — just keep it open.

---

## ⚠️ Disclaimer

Lumo is an independent, community-driven open-source project written from scratch. It targets the **Waveshare ESP32-S3-Touch-AMOLED-1.75** development board — a publicly available component sold by Waveshare. Lumo is **not affiliated with, endorsed by, or derived from** any commercial desk robot product or its creators.

Any 3D-printed enclosure, character design, or accessory you use Lumo with is your own responsibility. Lumo is firmware, not a product.

---

## ✨ Features

**Expressive eyes engine**
- Seven emotions: neutral, happy, angry, surprised, sleepy, scared, love
- Smooth frame-rate-independent animations with blinking, shake, and look targets
- Natural idle behavior: random saccades, spontaneous blinks
- Multiple eye styles (capsule, heart) — easily extended
- Configurable screen orientation — mount Lumo in any direction

**Reactive interaction**
- Touch gestures: tap, double-tap, swipe, tap-spam detection
- Pickup detection via 6-axis IMU — Lumo gets startled when lifted
- Clap detection via the onboard microphones
- USB plug/unplug events trigger emotion changes
- Battery-aware mood — Lumo gets sleepy when low on power

**On-device menus** (swipe left from the main screen)
- **Status** — battery percentage, voltage, charging state
- **Volume** — live preview, persisted across reboots
- **Eye color** — HSV color ring
- **Settings** — language, screen orientation, date and time

**Audio**
- Reaction sounds played through the onboard speaker
- Volume persisted across reboots
- Live clap-triggered reactions

**Power & persistence**
- Deep-sleep after idle, wake on touch
- Settings stored in NVS (volume, color, language, orientation)
- RTC keeps time through power cycles

**Multilingual**
- 🇬🇧 English · 🇳🇱 Dutch · 🇪🇸 Spanish
- Switchable from the on-device Settings menu — no reflash required
- More languages welcome via PR

---

## 🔧 Hardware

| Board | Status |
|---|---|
| [Waveshare ESP32-S3-Touch-AMOLED-1.75](https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm) | ✅ Fully supported |
| [Waveshare ESP32-S3-Touch-AMOLED-1.75C](https://www.waveshare.com/esp32-s3-touch-amoled-1.75c.htm) *(aluminum case variant)* | ✅ Fully supported |
| Other Waveshare AMOLED boards | 🛠️ Planned |

The target board features a 466×466 round AMOLED, capacitive touch, 6-axis IMU, dual microphones, speaker, RTC, and the AXP2101 power management IC. It runs an ESP32-S3 with 8 MB PSRAM and 16 MB flash — plenty of room for Lumo and your own extensions.

---

## 🚀 Flash it (for users)

The easiest way to install Lumo:

1. Open **[aymo.github.io/Lumo](https://aymo.github.io/Lumo/)** in **Chrome**, **Edge**, or **Opera** on a desktop
2. Plug your board into your computer with a USB-C data cable
3. Click **Install Lumo** → select your board's serial port → click **Install**
4. Done. Lumo starts running.

> Web Serial isn't supported in Firefox or Safari, or on mobile browsers. You need a desktop Chromium-based browser.

Prefer command-line flashing? Grab the `.bin` from [Releases](../../releases) and use `esptool.py`:

```bash
esptool.py --chip esp32s3 --port COMx write_flash 0x0 lumo-v0.1.0.bin
```

---

## 🛠️ Build it (for developers)

Lumo is built with [PlatformIO](https://platformio.org/) on the [pioarduino](https://github.com/pioarduino/platform-espressif32) fork — required for Arduino-ESP32 v3.x, which the CO5300 display driver depends on.

```bash
git clone https://github.com/Aymo/Lumo.git
cd Lumo

# Build & flash
pio run -t upload

# Monitor serial output
pio device monitor --baud 115200
```

**Windows-specific notes:**
- Run PlatformIO commands from **PowerShell** (not Git Bash). The pioarduino installer refuses MSYS environments.
- The build uses `core_dir = D:/p` in `platformio.ini` to work around Windows's MAX_PATH limit. Adjust to your setup if needed.

---

## ⚙️ Configuration

Most behavior is tweakable without recompiling — through the on-device menus or by editing constants in `src/` before building.

| Setting | Where | Default |
|---|---|---|
| Eye color | Eye color menu | Orange |
| Volume | Volume menu | 80% |
| Language | Settings menu | English |
| Screen orientation | Settings menu | 0° |
| Date and time | Settings menu | RTC-backed |
| Idle-to-sleep timeout | `src/main.cpp` | 5 min |
| Clap detection threshold | `src/audio.cpp` | 8000 |

Adding new languages, emotions, or eye styles is straightforward — see the structure of `src/lang/`, `src/emotion.cpp`, and `src/eye_engine.cpp` respectively.

---

## 🗺️ Roadmap

**Working today**
- ✅ Eye rendering engine with 7 emotions and 2 eye styles
- ✅ Touch, tap-spam, pickup, USB events
- ✅ Clap detection via onboard mics
- ✅ Audio reactions through onboard speaker
- ✅ On-device menus (status, volume, eye color, settings)
- ✅ Configurable orientation, language, date and time
- ✅ Deep-sleep with touch wake
- ✅ Multilingual (EN/NL/ES)

**Planned**
- Tilt-to-look using the gyroscope
- Wi-Fi provisioning + NTP time sync
- OTA firmware updates
- Web-based configuration portal
- Support for additional Waveshare AMOLED boards
- Optional AI personality layer

---

## 🤝 Contributing

Pull requests, issues, and ideas are welcome. Some easy ways to help:

- **Add a language** — see `src/lang/` for the existing English, Dutch, and Spanish files
- **Add a new emotion** — see how the seven existing ones are defined in `src/emotion.cpp`
- **Add a new eye style** — see the capsule and heart implementations in `src/eye_engine.cpp`
- **Share your build** — post a photo or video in [Discussions](../../discussions)
- **File a bug** — use the [issue tracker](../../issues)

For larger changes, please open a discussion or issue first so we can talk through the approach.

---

## 📜 License

Lumo is licensed under the **GNU General Public License v3.0**. See [`LICENSE`](LICENSE) for the full text.

In short: you may use, study, modify, and distribute Lumo freely. If you distribute a modified version, your modifications must also be open-source under GPL-3.0. This keeps Lumo, and everything built on it, open for everyone.

---

## 🙏 Credits

Lumo stands on the shoulders of:

- [**Waveshare**](https://www.waveshare.com/) — for the ESP32-S3-Touch-AMOLED-1.75 hardware and reference drivers
- [**Arduino_GFX**](https://github.com/moononournation/Arduino_GFX) by moononournation — graphics library
- [**SensorLib**](https://github.com/lewisxhe/SensorLib) and [**XPowersLib**](https://github.com/lewisxhe/XPowersLib) by lewisxhe — IMU and power management
- [**Espressif**](https://www.espressif.com/) — for the ESP32-S3 chip and the IDF
- [**pioarduino**](https://github.com/pioarduino/platform-espressif32) — PlatformIO platform fork supporting Arduino-ESP32 v3.x
- [**RoboEyes**](https://github.com/FluxGarage/RoboEyes) by FluxGarage — conceptual inspiration for the emotion-driven eye system
- [**ESP Web Tools**](https://esphome.github.io/esp-web-tools/) — browser-based flashing
- The ES8311 codec driver included in `lib/es8311/` is derived from Waveshare and Espressif example code

Lumo was developed with assistance from [Claude Code](https://www.anthropic.com/claude-code).

---

<sub>**Lumo** — *Lightweight Universal Moji OS* · Built with care for the maker community.</sub>