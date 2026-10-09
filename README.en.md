<p align="center">
  <img src="assets/banner.en.svg" alt="VesevOS - one platform, a thousand boards, each with its own identity" width="100%">
</p>

<p align="center">
  <img alt="License" src="https://img.shields.io/badge/license-GPL%20v3%20%2B%20commercial-3fa7ff">
  <img alt="Board" src="https://img.shields.io/badge/board-ESP32--S3-37e0b0">
  <img alt="Arduino" src="https://img.shields.io/badge/Arduino%20IDE-esp32%20core%203.3.x-00979d">
  <img alt="Status" src="https://img.shields.io/badge/status-in%20development-orange">
  <img alt="Languages" src="https://img.shields.io/badge/languages-it%20%C2%B7%20en%20%C2%B7%20es%20%C2%B7%20de-blueviolet">
</p>

<p align="center"><a href="README.md">Italiano</a> · <a href="README.en.md"><b>English</b></a></p>

<p align="center"><b>A small operating system for the ESP32-S3: easy to use, secure by default, and useful even where there is no internet.</b></p>

---

## In a nutshell

Plug in the board, open a page in your browser, and you already have a small operating system:
Wi-Fi, clock, files, LEDs, pins, a shell and much more, all protected by a password you choose.
**No cloud, no account, no subscription.** The web page and the languages live inside the board.

<p align="center">
  <img src="assets/screenshot-home.en.png" alt="The Home: status rings, board identity card and resources" width="48%">
  <img src="assets/screenshot-auto.en.png" alt="Automations: When, If, Then rules" width="48%">
  <img src="assets/screenshot-avvio.en.png" alt="Startup order of the services and the bottom terminal panel" width="48%">
  <img src="assets/screenshot-telefono.en.png" alt="The page on a phone, with the icon bar at the bottom" width="24%">
</p>
<p align="center"><sub>Screenshots of the control page (with sample data): Home in dark theme, Automations, startup order with the bottom terminal panel, and the phone layout.</sub></p>

> **Status:** in development (version 1.7.39). The project grows in stages: see "Where we are going".

## Why "VesevOS"?

**Vesevo** (Latin *Vesevus*) is the ancient name of **Mount Vesuvius**, the volcano that has always watched over the Bay of Naples.
We liked it because it tells the idea well: an always-lit energy, small but reliable, that stays in place
even when everything around falls silent. Hence the volcano in the banner.

## Mission

> **To make it simple and safe to turn an ESP32 board into a useful device, even for people just starting out.**

Beginners in electronics often hit a wall: libraries to choose, settings to write, security to invent.
VesevOS removes the wall. It gives you a ready-made, well-built foundation, explained in plain words,
on which to build what you need.

## Vision

> **A common platform for a thousand boards, where each one has its own identity and its own function, chosen by the user.**

Today VesevOS is the **standard base**: the same core for every board. Tomorrow each board will have its own
role (weather station, sensor node, repeater, communication point...) with a guided path to choose the function,
connect "plug and play" accessories and put it to work. We are thinking of boards that work
**even where the network does not reach**: in the countryside on a solar panel, in an emergency,
in radio silence when needed, or communicating in alternative ways.

## Philosophy

| | Principle | What it means |
|---|---|---|
| 🧭 | **Simple first** | Plain words, guided paths, nothing you are forced to understand. Dig deeper if you want to. |
| 🔐 | **Secure by default** | The password protects the page, the shell and the API. Anything risky asks for confirmation. |
| 📴 | **Works without internet** | No cloud dependency. Page, languages and licenses are inside the board. |
| 🔍 | **Open and transparent** | Readable code, no hidden tracking. If statistics are ever needed, they will be **only with your consent**. |
| 🧩 | **Modular** | Only what you need is switched on: every service has its own switch and uses power only when active. |
| 🌍 | **For everyone** | Italian, English, Spanish and German, and a new language is just one file. |
| 🔋 | **Reliable and frugal** | Built to survive blackouts and to use little power, even on battery. |

## What's new in 1.7.39

- **Bluetooth, your way**: on whenever you want, no countdown. It carries your board's name, and your phone pairs with a 6-digit code.
- **Tidier memory**: after switching Bluetooth on and off, RAM goes back to how it was (largest free block: 79 KB, it used to drop to 47).
- **Event Bus**: the board tells you what happens (network up/down, blocked IPs, alarms, services) and apps read it with `GET /api/events` or the `events` command.
- **Clean, documented API**: 127 routes, proper HTTP codes, documented in Italian and English ([docs/API.en.md](docs/API.en.md)).
- **Layered code**: drivers apart from services, small files, 8 groups of tests on the computer.
- **Accessible**: the page meets WCAG 2.2 AA.

## What it does today

- **Network**: AP mode (the board creates its own Wi-Fi network) and client mode, automatic or fixed IP, network scan, configurable host name and domain (`name.local` with mDNS).
- **Web page** with categories Home, Network, Services, Peripherals, System, Security; **guided first setup** (6 steps, with home Wi-Fi check).
- **Pin tests**: tap a pin to set an output high or low, make it blink or read it, with warnings about the risks. The test switches off by itself.
- **Shell**, on the web and on the serial port, with many commands (type `help`).
- **Security**: random hotspot password for every board, no factory panel password, HMAC sign-in (the password never travels), **HTTPS** with a unique certificate, up to 8 **users with roles**, per-IP lock-out, **IP filter**, **configuration checks** with alarms and change log.
- **Services** (off by default): **MQTT** (also encrypted, mqtts), **board network** over ESP-NOW with signed messages and up to 3 hops, **Bluetooth** for phones and apps (turned on by the page, the shell or an app).
- **Watchdog**: restarts hung services or the board, with an anti-loop limit.
- **Localization**: country, radio channels and power following the country's rules, antenna, time zone, formats.
- **Time**: NTP, manual date and time, local NTP server.
- **Files**: internal memory (LittleFS) with folders, upload/download/edit.
- **CPU**: automatic or fixed speed (80/160/240 MHz), internal temperature, alarm when too hot.
- **LED**: WS2812 RGB LED (system status, heartbeat tied to CPU load, fixed color).
- **Configuration** in OpenWrt style (`/vesevos.conf`), downloadable and restorable.
- **Languages**: Italian and English inside the firmware (with flags; keys 1 and 2 switch language on the serial); Spanish and German as separate files (`lang/`).
- **Legal notes**, software list (SBOM) and manual in the page and in the shell (`legal`, `license`). Manual: [docs/manual.en.md](docs/manual.en.md).

## Where we are going

These are ideas and plans, not promises: the order may change.

**Next releases**
- [ ] **"Control room" home page**: tiles for CPU, memory, disk, temperature, Wi-Fi signal and alarms; one click opens the right tab. Polished graphics with light SVG icons, light, dark or automatic theme.
- [ ] A single **Network** tab (Wi-Fi, IP, name, access point) and a dynamic, sortable **Tasks** tab.
- [ ] A simple **firewall**: blocking of repeated failed logins, rules for addresses, limits on connections.
- [ ] **Advanced pins**: voltage and frequency measurements, PWM, correction of values with a multimeter, animated pins.
- [ ] A more comfortable **web shell** (history, completion, colors).
- [ ] **Firmware update from the page** (OTA).

**Further ahead**
- [ ] **Guided first-time setup** (language, password, country, Wi-Fi, time).
- [ ] **SD card** and **Lua apps** confined to a "cage" on the SD card.
- [ ] **Plug-and-play accessory catalog**: pick the sensor from a list and VesevOS shows how to wire it and installs what is needed.
- [ ] **SD card** (1.8.0) and **OTA update from the page** (1.8.1).
- [ ] **Radio silence mode** and **power saving** for solar-powered use.
- [ ] **SSH** server.

## Requirements

- An **ESP32-S3 SuperMini** board (ESP32-S3FH4R2: 4 MB flash, 2 MB PSRAM).
- Arduino IDE with **esp32 core 3.3.x** or newer.
- Libraries (Arduino IDE Library Manager): **PsychicHttp** (3.1.x, MIT) and **ArduinoJson** (7.x, MIT). ESPAsyncWebServer and AsyncTCP are no longer needed.
- Bluetooth can be left out to save memory: `#define VOS_WITH_BLE 0` in `vos_common.h`.

## Arduino IDE settings

| Item | Value |
|---|---|
| Board | ESP32S3 Dev Module |
| PSRAM | QSPI PSRAM |
| USB CDC On Boot | Enabled |
| Partition Scheme | Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS) |

## Installation

1. Download or clone the repository.
2. Open the `VesevOS/` folder (it must contain `VesevOS.ino`) with Arduino IDE.
3. Set the board as in the table, then upload the sketch.
4. Open the serial monitor at 115200 baud to see the start-up messages.

## First access

1. Open the serial monitor (115200) and press RESET: the board prints the network name (**VesevOS**) and **its own password**,
   different for every board (required by device security laws: EU RED/EN 18031, Cyber Resilience Act; UK PSTI).
2. Join that network and open `http://192.168.4.1` (it usually opens by itself).
3. Choose the panel password (at least 6 characters) and follow the guided setup (rainbow LED).

**BOOT button** (hold while running, then release): under 2 s leaves airplane mode; 2-7 s (light blue LED) turns off the IP filter;
8-19 s (yellow LED) clears the administrators' passwords and prints the Wi-Fi password on the serial; 20 s or more (red LED) factory reset.
Do not hold it at power-up: the board would enter download mode. Everything is explained in the [manual](docs/manual.en.md).

## RGB LED colors ("Status" mode)

| Color | Meaning |
|---|---|
| Orange | Starting up |
| Blue | AP mode (own network) |
| Yellow | Connecting to Wi-Fi |
| Green | Connected to Wi-Fi |
| Flashing red | Alarm (high temperature) |

## Repository layout

```
VesevOS/      Arduino sketch (VesevOS.ino + .h/.cpp files)
web/          web page in HTML (source of vos_page.h, gzip-compressed)
docs/         manual, known issues (KNOWN_ISSUES) and release guide (RELEASE_DEVELOPER), in Italian and English
assets/       README banner and screenshots
lang/         language files (es, de) - GENERATED from tools/lang_src.json; English is inside the firmware
licenses/     license texts (GPL, LGPL, Apache, MIT, BSD) and legal notice template
tools/        mkpage.py (page), mklang.py (languages), mklicense.py (legal texts), mkcommon.py (country list),
              stub/ (check every file without a board: compila.sh), data/ (country data source)
NOTICE.txt    legal notes: ownership, licenses, regulatory references
SBOM.spdx.json list of the software used (SPDX format)
SECURITY.md   how to report security problems, years of support
CHANGELOG.md  version history (in Italian)
```

If you edit `web/index.html`, regenerate the page with:

```
python3 tools/mkpage.py
```

The firmware sources use only ASCII characters.

## Languages

Italian is inside the firmware. For the other languages:

1. Open the page, tab **System > Localization** (Languages card).
2. Choose the file (`en.json`, `es.json` or `de.json` from the `lang/` folder) and press **Upload language**.
3. Pick the language from the menu at the top right. The board remembers it; from the shell: `lang en`.

Each language takes about 17 KB of internal memory. **To add a language**: copy an entry of
`tools/lang_src.json`, add a column with the new code under `langs` and in the entries, then run
`python3 tools/mklang.py`. The check warns you if any text is missing.

## Contributing

Ideas, tests on real boards and fixes are welcome. A translation too: it is just one file in `lang/`.
For code, read `CONTRIBUTING.md`.

## License

Dual license:

- **GPL v3 or later** (file `LICENSE`): free. Anyone who distributes a modified VesevOS, or one inside a
  product, must publish their code under the same license.
- **Commercial license**: for using VesevOS in closed products. See `COMMERCIAL.md`.

Ownership, third-party library licenses and regulatory references: see `NOTICE.txt` (also on the
page, System > Legal notes tab, and in the shell with `license`).

Copyright (C) 2026 Domenico Paolella.
