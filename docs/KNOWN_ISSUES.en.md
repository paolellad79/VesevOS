# VesevOS - Known Issues

Last update: 8 October 2026, version 1.7.11. This file is also published on GitHub (docs/) and updated at every release.
Status: **O** = open, **M** = mitigated (a workaround exists), **S** = to fix before selling, **W** = watching.

## Memory (RAM)
| # | Problem | Status | What to do |
|---|---|---|---|
| M1 | After many Bluetooth start/stop cycles the RAM gets fragmented: the largest free block drops (from ~83 KB to ~39 KB) and `esp-aes: Failed to allocate memory` may appear. | M | Since 1.7.8f: 5 s pause between two starts and a 45 KB minimum block. Since 1.7.9a: `free detail` command and a yellow "Fragmented memory" alarm. To recover: reboot the board. The real fix (buffers in PSRAM, Bluetooth as a package) is still to do. |
| M2 | Low free RAM at idle (about 125 KB; much less with an HTTPS page open). The sketch is near the limit: the "Huge APP" partition is required. | O | Do not switch everything on at once. Reduction plan (String -> fixed buffers, packages with #define) is part of the refactoring. |
| M3 | There is no room for two 1.9 MB OTA slots with the current partition. | O | OTA (1.8.1) must be rethought. |
| M4 | With a service compiled in but **switched off**, the RAM given back is not measured yet (`ram mark` / `ram diff`). Packages set to 0 really remove code and RAM; switching off from the page may free less. | A | Measure on the board; see `claude/VesevOS-valutazione-core-app.md`. |

## Stability
| # | Problem | Status | What to do |
|---|---|---|---|
| S1 | "Task watchdog" after a USB reset (seen about 7 s after boot). | W | Not seen again in more than 11 boots since 1.7.8c. The reboot diary (`reboots`) records the `L:...` stage where it stopped. If it happens: send `reboots` and `log 30`. |
| S2 | On 4 Oct the watchdog fired when pressing "Regenerate key" while switching on the board-to-board network (ESP-NOW). Not reproduced. | W | Repeat in separate steps (first "Regenerate key", then "Switch on") with the serial port open; the build .elf file is needed to read the backtrace. |
| S3 | 1.7.11 (Device Manager and `/api/dev` API, packages, encrypted backup, watchdog reduced mode, serial welcome without repeats) was checked with stubs, host tests (144) and the page with a fake server, **not yet on the board**. | A | Try: Device Manager, `dev list`, Backup and restore (with and without secrets), Radio page, opening the serial monitor on an unconfigured board (the welcome must not keep scrolling). |
| S4 | Watchdog reduced mode (after 3 restarts in one hour): written and checked in code only, not yet triggered on the board. Switching Bluetooth off from the watchdog task has not been measured. | A | If the board restarts by itself several times: send `reboots` and the full "Task watchdog got triggered" message. |

## Security (to fix before selling)
| # | Problem | Status | What to do |
|---|---|---|---|
| X1 | Only HTTP can be used (user's choice, with a warning). | S | HTTPS by default and HTTP off when Wi-Fi is connected. |
| X2 | The admin password is shown in clear text on the serial port during `setup`. Physical access to the board is needed. | S | Different password per board (label/QR), mandatory change at first login. |
| X3 | The HTTPS certificate is self-signed: the browser warns "connection not private". | S | Compare the SHA-256 fingerprint (Services > HTTPS page, `cert` command). Plan: renewal, fingerprint in the QR, product CA (claude/VesevOS-piano-giallo-verde.md). |
| X4 | CRA procedure (vulnerability reports, updates), trademarks, export classification, LGPL of the Arduino-ESP32 core, `posix_tz_db` data licence: paperwork still open. | S | See the "yellow to green" plan. This is not legal advice. |
| X5 | TLS buffers live in unencrypted PSRAM (decrypted HTTPS session data). Flash/PSRAM encryption or buffers in internal RAM are needed. | S | Before selling. |
| X6 | Backup with secrets: security depends on the chosen phrase (minimum 10 characters, 20000 PBKDF2 rounds). A weak phrase can be guessed by trying on a computer. Lost phrase = lost secrets. | A | Choose a long phrase. The warning is already on the page. |
| X7 | External apps (Lua, future) are not signed yet: SHA-256 is used in development. The effect of third-party apps on CE/CRA conformity must be asked of a consultant. | S | Before selling. |

## Features
| # | Problem | Status | What to do |
|---|---|---|---|
| F2 | Boards configured before 1.7.9a with NTP switched off by the setup guide do not carry the "switched off by the guide" mark: NTP does not switch itself on. (From 1.7.9a this works for new guides.) | M | Switch NTP on by hand in Services. |
| F3 | In sleep mode the BOOT button does not wake the board (timer or RESET only). Wi-Fi and the page are off. | O | Declared limit. |
| F4 | Statistics do not count pins or power use. | O | Declared limit. |
| F5 | USB serial (CDC): baud rate cannot be changed (ignored); it only applies to a UART port. The speed trial lasts 20 s and must be confirmed with `serial keep`. | O | Normal for native USB. |
| F6 | Serial line ending: PuTTY needs "CR+LF" (default) to avoid staircase lines; change it in System > Serial or with `serial eol`. | M | |
| F7 | Bluetooth buttons may stay grey for a few seconds after "stop" (5 s pause). | M | Wait. |
| F8 | "Task watchdog" reboots seen on the board (even after 13 hours) with minimum RAM under 40 KB. 1.7.9a removes the likely causes (blocking serial, idle HTTPS connections) and the diary now says more. | O | Send `reboots` and the "Task watchdog got triggered" message from the serial monitor. |

## Building
(Fixed in 1.7.9a: F1 `wifi set` with spaces, C1 `inList` warning.)

| # | Problem | Status | What to do |
|---|---|---|---|
| C2 | PsychicHttp 3.1.2 and ArduinoJson 7 are required; it does not build without PsychicHttp. | O | Install them from the Library Manager. |

## How to report
Send: version (`ver`), `reboots`, `log 30`, `free detail` and what you were doing. Do not paste passwords or keys.
