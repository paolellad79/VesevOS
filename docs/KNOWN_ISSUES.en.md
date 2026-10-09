# VesevOS - Known Issues

Last update: 8 October 2026, version 1.7.39. This file is also published on GitHub (docs/) and updated at every release.
Status: **O** = open, **M** = mitigated (a workaround exists), **S** = planned for the commercial edition, **W** = watching.

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
| S5 | 1.7.12 splits Bluetooth into three files (driver `vos_drv_ble`, life cycle `vos_ble`, commands `vos_ble_cfg`). Checked with stubs and host tests (commands), **not on the board**. | A | Try: `ble on`, pairing with the code, commands `status`, `wifi`, `done` from a phone, switch-off after 10 minutes, `ble off` and switching on again after 5 seconds. Free RAM after switch-off the same as before. |
| S6 | 1.7.13 moves the files to `VesevOS/src/<layer>/`. Checked with stubs and host tests, **not with Arduino IDE**. | A | Build the sketch from the `VesevOS/` folder as usual. If the IDE says "No such file" or "was not declared" send me the whole error (an include path may be needed). Also check the sketch size is the same as 1.7.12 (no code changed). |
| S7 | 1.7.14 adds the service registry (`vos_service`) and registers Bluetooth in it. Checked with stubs and host tests, **not on the board**. | A | Try `ble on`, `ble off`, `ble on` after 5 s, and on/off from the page and from `dev`. Expected: identical to 1.7.13. |
| S8 | 1.7.15 splits ESP-NOW into driver (`vos_drv_espnow`) and service (`vos_mesh`). Checked with stubs, **not on the board** (a second board is needed to see messages). | A | With 2 boards: `mesh start` on both, check they see each other (`mesh`), send `mesh send <name> hi`, try `mesh stop`/`start` and the same from the page. Expected: identical to 1.7.14. |
| S9 | 1.7.16 splits MQTT into driver (`vos_drv_mqtt`) and service (`vos_mqtt`). Checked with stubs, **not on the board**. | A | With a broker: start/stop/restart from the page and with `mqtt start|stop|restart`, check `state` and `cmd` (e.g. `led-color ff0000`), try with and without TLS, change settings while running. Expected: identical to 1.7.15. |
| S10 | 1.7.17 moves all Wi-Fi calls of `vos_net` into the `vos_drv_wifi` driver. Checked with stubs, **not on the board**: this is the most delicate part (if wrong the board does not connect or start the hotspot). | A | Try: boot with home network (connects, correct IP); boot without network (hotspot, password from the label); static IP; network scan from the page; home-network test in the guide; hotspot channel change with ESP-NOW; airplane mode on/off; unplug and replug the router. If something fails: BOOT 8 s (recovery) and send me the log. |
| S11 | 1.7.18 ties DHCP to the hotspot. Checked with stubs, **not on the board**. | B | Try: finish the guide with home network (hotspot and DHCP off); turn hotspot back on from Network (DHCP on); without home network the hotspot hands out addresses; already configured board: DHCP off after boot. |
| S12 | 1.7.19 moves country/power, power saving and signal readings into the Wi-Fi driver. Checked with stubs, **not on the board**. | B | Try: Radio page (country and power applied, RADIO log line), power-saving mode, signal on Home, Selftest (Wi-Fi check), deep sleep. |
| S13 | 1.7.20 moves all file operations into the `vos_drv_fs` driver. Checked with stubs, **not on the board**: it touches configuration and diary. | A | Try: normal boot (config loaded); save a setting and reboot; System > Files (list, new folder, write, rename, copy, delete, download, upload); language change; diary after reboot; Selftest (memory check); MQTT certificate. If the config disappears: BOOT 8 s and send me the log. |
| S14 | 1.7.21 moves pinMode/digitalWrite/digitalRead into the `vos_drv_gpio` driver. Checked with stubs, **not on the board**. | B | Try: BOOT button (pressed now in Device Manager; BOOT 8 s = recovery); Pins > test a free pin (high, low, blink, read with pull); a rule that drives a pin; Selftest (button check). |
| S15 | 1.7.22 adds `apiVersion` and the `code` field in errors. Checked with stubs and the fake page, **not on the board**. | B | Try: the page works as before (login, errors shown as text); `/api/status` shows `apiVersion:1`; wrong password and insufficient role show the message and carry a `code`. |
| S16 | 1.7.23 moves `/api/mesh/key` from GET to POST. Checked with stubs and the fake page, **not on the board**. | B | Try: Board network > show key (it appears); "MESH: chiave mostrata" line in the log; other boards still connected. |
| S17 | 1.7.24 makes errors answer with the right HTTP status (400/403/404/409/413/429). Checked with stubs and the fake page, **not on the board**: the page must show messages as before. | A | Try: wrong password (red message, does NOT return to the login screen); too many attempts (block); invalid value in a tab (wrong IP); turn off the hotspot without home Wi-Fi (message); file too large; guide: weak password. An empty error must never appear. |
| S18 | 1.7.25 splits `vos_web.cpp` into six files. Moves only, checked with stubs and the fake page, **not on the board**. | B | Try: it compiles; the page works as before (login, Home, Network, Devices, Files, Selftest). Sketch size should be almost the same as 1.7.24 (tell me). |
| S19 | 1.7.26 splits `vos_shell.cpp` into four files. Moves only, checked with stubs, **not on the board**. | B | Try from serial and web shell: help, status, ls/cat, wifi, led, pin, svc, user, mesh, mqtt, power, reboot. Tell me the sketch size. |
| S20 | 1.7.27 splits `vos_config.cpp` into three files. Moves only, checked with stubs and host tests, **not on the board**. | B | Try: it compiles; settings survive a reboot; export/import configuration works. Tell me the sketch size. |
| S21 | 1.7.28 splits the page source into `web/src/`. The final page is identical to 1.7.27 (compared byte for byte), checked with stubs and the fake page. | B | Try: it compiles; the page works as before. Sketch size equal to 1.7.27 (2015923). |
| S22 | 1.7.29 limits the file list (200 items) and found networks (40) and adds `apiVersion` to `/api/common`. Checked with stubs and the fake page, **not on the board**. | B | Try: Files > a normal folder looks as before; Network > scan works. With a folder of more than 200 files the "Partial list" notice appears. Tell me the sketch size. |
| S23 | 1.7.30 moves the temperature in the CPU widget below the orange graph. Checked with the fake page (image checked), **not on the board**. | B | Look at Home: under CPU only the MHz; under the orange graph "Temp. xx.x °C". |
| S25 | The Event Bus (1.7.32-1.7.33) is confirmed on the board for `svc.state`, `net.state`, `sec.ban`, `sec.unban`. Only `audit.new` / `audit.clear` remain to be seen. | B | At the first yellow alert (e.g. `frag`, memory fragmented after Bluetooth) run `events`: `audit.new` must appear with the alert code; when it clears, `audit.clear`. |
| S28 | 1.7.38 adds the events `mqtt.link`, `ble.link`, `mesh.node`, `time.sync`. Tried only with stubs, **not on the board**. | B | Shell `events` after: connecting the phone to Bluetooth (`ble.link 1`, then 0 on disconnect); MQTT on/off with a reachable broker (`mqtt.link`); NTP sync (`time.sync`, also with `ntp sync`). With two boards: `mesh.node` when a neighbour appears. |
| S29 | 1.7.39 runs a dry Bluetooth start at boot so that after the first `ble on`/`ble off` the largest RAM block does not drop from 79 to 47 KB. Tried only with stubs, **not on the board**. | A | In the boot log look for "BLE: memoria preparata al boot" (before/after). Then `free detail` at rest (before: 121 KB free, largest block 79 KB), `ble on` + `free detail`, `ble off` + `free detail`: the largest block after switching off must stay close to the one at rest (before: 47 KB). Check boot is not slower by more than 1 second and Bluetooth turns on as before. If the board restarts by itself at boot: set `VOS_BLE_PRIME 0`. |

## Security (planned for the commercial edition)
| # | Problem | Status | What to do |
|---|---|---|---|
| X1 | Only HTTP can be used (user's choice, with a warning). | S | HTTPS by default and HTTP off when Wi-Fi is connected. |
| X2 | The admin password is shown in clear text on the serial port during `setup`. Physical access to the board is needed. | S | Different password per board (label/QR), mandatory change at first login. |
| X3 | The HTTPS certificate is self-signed: the browser warns "connection not private". | S | Compare the SHA-256 fingerprint (Services > HTTPS page, `cert` command). Plan: renewal, fingerprint in the QR, product CA (claude/VesevOS-piano-giallo-verde.md). |
| X4 | CRA procedure (vulnerability reports, updates), trademarks, export classification, LGPL of the Arduino-ESP32 core, `posix_tz_db` data licence: paperwork still open. | S | See the "yellow to green" plan. This is not legal advice. |
| X5 | TLS buffers live in unencrypted PSRAM (decrypted HTTPS session data). Flash/PSRAM encryption or buffers in internal RAM are needed. | S | Planned. |
| X6 | Backup with secrets: security depends on the chosen phrase (minimum 10 characters, 20000 PBKDF2 rounds). A weak phrase can be guessed by trying on a computer. Lost phrase = lost secrets. | A | Choose a long phrase. The warning is already on the page. |
| X7 | External apps (Lua, future) are not signed yet: SHA-256 is used in development. The effect of third-party apps on CE/CRA conformity must be asked of a consultant. | S | Planned. |

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
