# VesevOS - Release Developer (guide for people who develop and release)

Last update: 10 October 2026, version 1.7.45. Also published on GitHub (docs/). The step-by-step procedure is in the release checklist (kept in the Project); this file gives the overall picture for people working on the code.

## 1. Environment
- Board: ESP32-S3 SuperMini (4 MB flash, 2 MB PSRAM, native USB, WS2812 LED on GPIO48, internal antenna only).
- Arduino IDE, Arduino-ESP32 core 3.3.x, "Huge APP" partition (compiler only; the real table is in recovery/VesevOS_Recovery/partitions.csv: 1 MB recovery + 2.31 MB app0). A release also publishes recovery/ (sketch, scripts, README).
- Libraries: PsychicHttp 3.1.2, ArduinoJson 7 (mbedTLS and ESP-IDF come with the core).
- Licence: GPL-3.0-or-later or commercial licence (COMMERCIAL.md). The Arduino core (LGPL) is not shipped with the repo.

## 2. Layout
- `VesevOS/` firmware: `VesevOS.ino` + `src/` with one folder per layer: `core/` core, `security/` access and encryption, `net/` network, `devices/` Device Manager and pins/LED, `drivers/` hardware and radio libraries (`vos_drv_*`), `packages/` optional services, `interfaces/` web and shell. Includes are relative to the file. `vos_page.h`, `vos_common_data.h`, `vos_lang_en.h` and `vos_license_data.h` are GENERATED (in `src/interfaces/` and `src/core/`).
- `web/index.html` page; `lang/` generated languages.
- `tools/`: `lang_src.json` (it/en/es/de translations), `mklang.py`, `mkpage.py`, `mklicense.py`, `mkcommon.py`.
- `tools/stub/`: fake headers to compile on a computer. `tools/webtest/`: fake server + Playwright. `tools/hosttest/`: host tests (`run.sh`).
- `docs/`: manuals it/en, images, KNOWN_ISSUES (it/en), RELEASE_DEVELOPER (it/en).

## 3. Code rules
- Firmware sources are ASCII only. Texts go through `tr()`/`trf()` (firmware) and `t()`/`tf()` (page). Unique keys; identical placeholders in all languages.
- Every web route has a role (guest/operator/admin). Never put passwords, keys or codes in the log (guests can read it).
- Data touched by several tasks: mutex or `portMUX`. Long operations never in the loopTask; feed the watchdog.
- Memory: beware of fragmentation; before opening TLS or BLE check the largest free block (`ESP.getMaxAllocHeap()`).
- 500-line limit per file and no duplication (rules of the step-by-step refactoring).
- LAYERS (diagram in the Project: `VesevOS-schema-strati.md`): drivers (`vos_drv_*`, the only code allowed to touch hardware or a radio library) -> Device Manager (`vos_dev`) -> core -> packages/services -> interfaces (web, shell) -> apps. Each layer uses only the one below. A service asks the driver and never includes the hardware library. `python3 tools/checklayers.py` counts direct hardware accesses outside drivers: the starting debt is in `tools/layers_baseline.json` and can only go down (BLE is already at zero).

## 4. Build and test
1. `python3 tools/mklicense.py` (if missing), `python3 tools/mklang.py` (must end with "ok"), `python3 tools/mkpage.py`.
2. Compile ALL files with the stubs (g++ -c) and look for duplicate functions (nm).
3. `tools/hosttest/run.sh` (needs `$CORE` = the `cores/esp32` folder of the Arduino core). Currently 162 tests. `python3 tools/checklayers.py` must end without KO.
4. Page: `tools/webtest/server.js` + `smoke172.js` -> "TUTTO OK".
5. Real build and test on the board: done by the owner.

## 5. Versions
- New features = new number (1.7.11). Bug fixes only = a letter (1.7.9b).
- The version goes in: `vos_common.h`, `VesevOS.ino`, ZIP name, CHANGELOG, README it/en, SBOM, `mkcommon.py`, LEGGIMI, manuals.
- Architecture refactoring: one step = one version, never two layers at once.

## 6. Release
- First the legal traffic light and the owner's "ok"; then the two ZIPs: `VesevOS-X.Y.Z.zip` (to build) and `VesevOS-repo.zip` (for GitHub).
- Commit: title `X.Y.Z - ...`, description, line `Controllo legale: green/yellow/red`.
- The owner does the push. Afterwards: copy to the Project, update STATE and to-do.
- Documents are published in Italian and English every time.

## 7. Useful diagnostics
- Serial: `help`, `ver`, `free`, `free detail`, `reboots` (diary), `log 30`, `serial`, `cert`, `selftest`.
- The reboot diary records the stage (`L:start`, `L:shell`, `L:ble`, ... `L:wdt`) where the board stopped.

## 8. What we never do
No MapSVG, exFAT, same default password on every board, services without access control, libraries without a clear licence or incompatible with GPL-3.0 + commercial, personal data sent out without consent, channels or power above the country limit.

## 9. Related documents
- Known issues: `docs/KNOWN_ISSUES.en.md`.
- In the Project (not published): release checklist, traffic light, legal register, yellow-to-green plan, email plan, to-do, restart notes.
