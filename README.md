# Lumo

**Lightweight Universal Moji OS** — open-source firmware that turns a small ESP32-S3 AMOLED display into an expressive, emotional desk companion.

![Lumo demo](docs/img/lumo-demo.gif)

> ⚡ **Flash in your browser** → [**lumo.github.io/flash**](#) *(coming soon)*
> 📦 **Download firmware** → [Releases](../../releases)
> 💬 **Discuss & share** → [Discussions](../../discussions)

---

## ⚠️ Disclaimer

Lumo is an independent, community-driven open-source project. It is **not affiliated with, endorsed by, or derived from** any commercial desk robot product or its creators. Lumo is written from scratch for the **Waveshare ESP32-S3-Touch-AMOLED-1.75** development board — a publicly available component from Waveshare.

Any 3D-printed enclosure, character design, or accessory you use Lumo with is your own responsibility. Lumo is a firmware, not a product.

---

## What is Lumo?

Lumo is a configurable **expressive-eyes firmware** for round AMOLED displays on ESP32-S3 hardware. Think of it as a tiny face engine: it renders animated eyes that blink, look around, express emotions, and react to touch, motion, sound, and the world around them.

Use Lumo to build:

- A desk companion with personality
- A face for your own robot project
- An ambient mood display
- An interactive art piece
- Whatever else your maker brain comes up with

Lumo is **GPL-3.0 licensed**. Use it, modify it, share it — just keep it open.

---

## ✨ Features

**Expressive eyes engine**
- Seven built-in emotions: neutral, happy, angry, surprised, sleepy, scared, love
- Configurable eye shapes (capsule eyes, heart eyes, more coming)
- Smooth frame-rate-independent animations, blinking, shake, look targets
- Natural idle behavior with random saccades and blinks

**Interaction**
- Tap, double-tap and swipe gestures
- Pickup detection via IMU (gets scared when lifted)
- Audio output — plays reactions through the onboard speaker
- Clap detection *(experimental — see [Roadmap](#-roadmap))*

**Built-in menu**
- Battery status
- Volume control
- Eye color picker
- Time setting (RTC-backed)

**Power & persistence**
- Deep-sleep after idle, wake on touch
- Battery-aware mood (gets sleepy when low)
- Settings persisted across reboots (NVS)
- RTC keeps time through power cycles

**Configurable** (via on-device menu or `config.h`)
- Screen orientation
- Language *(English, Dutch — more via PRs welcome)*
- Default eye style & color
- Timeouts, thresholds, and animation speeds

---

## 🔧 Hardware

Lumo currently targets:

| Board | Support |
|---|---|
| [Waveshare ESP32-S3-Touch-AMOLED-1.75](https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm) | ✅ Fully supported |
| [Waveshare ESP32-S3-Touch-AMOLED-1.75C](https://www.waveshare.com/esp32-s3-touch-amoled-1.75c.htm) *(aluminum case)* | ✅ Fully supported |
| Other Waveshare AMOLED boards | 🛠️ Planned |

The board features a 466×466 AMOLED display, capacitive touch, 6-axis IMU, dual microphones, a speaker, RTC, and an AXP2101 power management IC. It runs on an ESP32-S3 with 8 MB PSRAM and 16 MB flash — more than enough for Lumo and your own extensions.

---

## 🚀 Quick start (for users)

No compiler, no toolchain — just flash from your browser.

1. Open [**lumo.github.io/flash**](#) in Chrome or Edge
2. Plug your board into USB
3. Click **Connect** → select the serial port → click **Install**
4. Done. Lumo starts running.

*Requires a Chromium-based browser with WebSerial support (Chrome, Edge, Opera). Firefox and Safari are not supported by WebSerial at this time.*

Prefer command-line flashing? Grab the `.bin` files from [Releases](../../releases) and use `esptool.py`.

---

## 🛠️ Quick start (for developers)

Lumo is built with [PlatformIO](https://platformio.org/) on the [pioarduino](https://github.com/pioarduino/platform-espressif32) fork (required for Arduino-ESP32 v3.x support).

```bash
# Clone
git clone https://github.com/YOUR-ORG/lumo.git
cd lumo

# Build & flash (PowerShell on Windows; bash on Linux/macOS)
pio run -t upload

# Monitor
pio device monitor --baud 115200
```

**Windows users:** run PlatformIO commands from **PowerShell**, not Git Bash. The pioarduino toolchain installer refuses MSYS environments.

See [`docs/BUILDING.md`](docs/BUILDING.md) for the full build guide, including the `core_dir` workaround for Windows MAX_PATH issues.

---

## ⚙️ Configuration

Most behavior is tweakable without recompiling — through the on-device menu (swipe left from the main screen) or by editing `src/config.h` before building.

| Setting | Where | Default |
|---|---|---|
| Screen orientation | `config.h` | 0° |
| Language | `config.h` | English |
| Eye style | Menu / `config.h` | Capsule |
| Default eye color | Menu / `config.h` | Orange |
| Idle-to-sleep timeout | `config.h` | 5 min |
| Clap detection threshold | `config.h` | 8000 |

For adding new eye styles, new emotions, or new languages, see [`docs/EXTENDING.md`](docs/EXTENDING.md).

---

## 🗺️ Roadmap

**Working now**
- Eye rendering engine with 7 emotions
- Touch, tap-spam, pickup, USB events
- Audio output (scream on pickup)
- Menu UI (battery, volume, color, time)
- Deep-sleep & wake

**In progress**
- 🔧 Clap-detection debugging (I²S RX path tuning)
- 🔧 Generic configuration layer (orientation, language, eye types)

**Planned**
- Tilt-to-look using gyroscope
- Wi-Fi provisioning + NTP time sync
- OTA updates
- Web-based configuration portal
- Support for other Waveshare AMOLED boards
- Optional AI personality layer (cloud or on-device)

---

## 🤝 Contributing

Pull requests, issues, and ideas are welcome. Some easy ways to help:

- **Add a language** — see `src/i18n/`
- **Add a new emotion or eye style** — see `docs/EXTENDING.md`
- **Share your build** — post in [Discussions](../../discussions) with a photo
- **File a bug** — use the [issue templates](../../issues/new/choose)

Please read [`CONTRIBUTING.md`](CONTRIBUTING.md) and [`CODE_OF_CONDUCT.md`](CODE_OF_CONDUCT.md) before your first PR.

---

## 📜 License

Lumo is licensed under the **GNU General Public License v3.0**. See [`LICENSE`](LICENSE) for the full text.

In short: you may use, study, modify, and distribute Lumo freely. If you distribute a modified version, your modifications must also be open-source under GPL-3.0. This keeps Lumo, and everything built on it, open for everyone.

---

## 🙏 Credits & acknowledgements

Lumo stands on the shoulders of:

- [**Waveshare**](https://www.waveshare.com/) — for the excellent ESP32-S3-Touch-AMOLED-1.75 hardware and reference drivers
- [**Arduino_GFX**](https://github.com/moononournation/Arduino_GFX) by moononournation — graphics library
- [**SensorLib**](https://github.com/lewisxhe/SensorLib) and [**XPowersLib**](https://github.com/lewisxhe/XPowersLib) by lewisxhe — IMU and power management
- [**Espressif**](https://www.espressif.com/) — for the ESP32-S3 chip and the IDF
- [**pioarduino**](https://github.com/pioarduino/platform-espressif32) — PlatformIO platform fork
- [**RoboEyes**](https://github.com/FluxGarage/RoboEyes) by FluxGarage — conceptual inspiration for the emotion system
- [**ESP Web Tools**](https://esphome.github.io/esp-web-tools/) — browser-based flashing

Lumo was developed with assistance from [Claude Code](https://www.anthropic.com/claude-code).

---

<sub>**Lumo** — *Lightweight Universal Moji OS* · Made with care for the maker community.</sub>
