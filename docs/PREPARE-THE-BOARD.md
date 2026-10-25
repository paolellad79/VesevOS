# Preparing the board, step by step (VesevOS 1.8.0b)

This guide takes a new (or reset) **ESP32-S3 SuperMini** to a working VesevOS with cable-free updates.
Time: about 40 minutes the first time. Examples are for macOS; on Linux and Windows only the port name changes (see step 3).
[Versione italiana](GUIDA-PREPARARE-LA-SCHEDA.md)

**How it works, in short.** Two programs live on the board: the **recovery** (small, used only to update) and **VesevOS**.
The first install is done over USB. After that you update VesevOS from the web page (user menu > *Update via recovery*),
with no cable and no SD card.

## What you need
- The ESP32-S3 SuperMini board (4 MB flash, 2 MB PSRAM) and a USB cable **that carries data** (not charge-only).
- A computer with Python 3 (for `esptool`). To build the files yourself you also need ESP-IDF: see the [README](../README.en.md), build section.
- The VesevOS repository downloaded from GitHub (*Code > Download ZIP*) and unzipped.
- The **three things to write**: the recovery (`VesevOS-Recovery-0.5.1.bin`), the firmware (`VesevOS-<version>.bin`, for example `VesevOS-1.8.0b.bin`)
  and the two boot files (bootloader and partition table) produced by building the recovery.
  The ready files are in the `firmware/` folder of the repository (with SHA-256 and `indice.json`); if you do not build, the bootloader and partition table are in `flash/bin/` (`bootloader-1.8.0.bin`, `partition-table-1.8.0.bin`).

## Step 1 - How the files are laid out and where they go
VesevOS uses ESP-IDF: no IDE is needed and there is no "Upload" button. Files are written with `esptool`, at **explicit addresses**.

| Address | Content | Maximum size |
|---|---|---|
| `0x0` | bootloader (`bootloader/bootloader.bin` in the recovery build folder) | - |
| `0x8000` | partition table (`partition_table/partition-table.bin` in the same folder) | - |
| `0x10000` | recovery (`VesevOS-Recovery-0.5.1.bin`) | 1,048,576 bytes |
| `0x110000` | firmware (`VesevOS-<version>.bin`) | 2,424,832 bytes |
| `0x360000` | `spiffs` (configuration files): **not written**, the firmware creates it | - |

The partition table is described in `idf-recovery/partitions.csv` (the recovery build project, next to the repository, not published today); the ready file is `flash/bin/partition-table-1.8.0.bin`.
Recovery 0.5.1 is 873,200 bytes, firmware 1.8.0b 2,212,400 bytes: both fit.

**Never write the firmware at `0x10000`**: it would erase the recovery. An IDE "Upload" button and `idf.py flash` write there: do not use them for VesevOS.

## Step 2 - Checking the files
The scripts in the `recovery/` folder use `recovery/controlli.sh`: it refuses the recovery in place of the firmware (and the other way round), the `.merged.bin` file
and files that are not ESP32-S3 programs (header, `{FW:VesevOS}` or `=== VesevOS Recovery` marker, size). To skip it: `FORZA=1` (not recommended).
Dry run (writes nothing): `bash -c '. recovery/controlli.sh; vos_chk_fw firmware/1.8.0b/VesevOS-1.8.0b.bin && echo ok'`.

## Step 3 - Upload tool (esptool)
1. In the terminal: `python3 -m pip install esptool`
2. Plug in the board and find the port: on macOS `ls /dev/cu.usbmodem*` (e.g. `/dev/cu.usbmodem14401`); on Linux `/dev/ttyACM0`; on Windows `COM5` (Device Manager).
3. Close any program that uses the port (serial monitor).
4. If the board does not answer: hold **BOOT**, press and release **RESET**, release BOOT, and try again.

## Step 4 - Backup (recommended)
**Flash backup** (about 8 minutes, once; lets you go back to how it was):
`python3 -m esptool --chip esp32s3 -p PORT -b 460800 read_flash 0 0x400000 ~/vesevos-backup-4MB.bin`

## Step 5 - Installing over the cable
- **New board, or start from scratch** (ERASES everything, including the configuration):
  ```
  python3 -m esptool --chip esp32s3 -p PORT -b 460800 erase_flash
  python3 -m esptool --chip esp32s3 -p PORT -b 460800 write_flash --flash_mode keep --flash_freq keep --flash_size 4MB \
    0x0 flash/bin/bootloader-1.8.0.bin 0x8000 flash/bin/partition-table-1.8.0.bin 0x10000 firmware/recovery/0.5.1/VesevOS-Recovery-0.5.1.bin 0x110000 firmware/1.8.0b/VesevOS-1.8.0b.bin
  ```
- **Firmware only**, on a board that already has the recovery (leaves recovery and configuration alone):
  `recovery/carica-firmware.sh PORT firmware/1.8.0b/VesevOS-1.8.0b.bin`
  (same as `write_flash 0x110000 <file>`, after the checks of step 2).

Always check you have not swapped the files: recovery at `0x10000`, firmware at `0x110000`.
If you prefer scripts: `recovery/installa.sh PORT FOLDER [FIRMWARE]` (new board, erases everything) and `recovery/carica-tutto.sh PORT FOLDER FIRMWARE` (without erasing); the folder is the ESP-IDF build of the recovery and holds `VesevOS_Recovery.bin`, `bootloader.bin` and `partition-table.bin`. The scripts make the same checks as step 2. A from-scratch first install with these files and scripts is NOT shown as tested on a board in a written report: back up first and try on a spare board.
If you see *"no sync reply"*: close the serial monitor, repeat step 3.4 and run it again.

**Without a terminal:** the first-install page `flash/` (enabled with GitHub Pages, see `flash/README.md`) writes the same files from the browser, using the serial port (Web Serial) with checks on SHA-256, marker and size. **NOT TESTED with a real board.**

## Step 6 - First start of VesevOS
1. Open the Serial Monitor (115200) and press RESET on the board. At start the recovery waits 3 seconds, then VesevOS starts.
2. Read the welcome screen: Wi-Fi network `VesevOS`, random password (12 characters) and address `http://192.168.4.1`.
3. Join that network with a phone or PC, open the page and **choose the panel password** (at least 6 characters).
4. Follow the **guided setup** (language, country, name, antenna, home Wi-Fi, time). Details: manual, chapter 1.

## Step 7 - Updating later (no cable)
1. Build the new version, or take the ready file from `firmware/` and check its SHA-256 (see `firmware/LEGGIMI.md`, in Italian).
2. Open the VesevOS page, sign in as **Administrator**, user menu > **Update via recovery** > *Restart into recovery*.
3. The page shows an address (for example `http://192.168.1.50:80/`). Open it in a **private browser window** and type **http**, not https.
   If within 30 seconds the board cannot find your home network, it creates the Wi-Fi network `VesevOS-recovery`: the password is on the serial monitor.
4. Sign in with the user and password of a VesevOS Administrator.
5. Choose `VesevOS-<version>.bin`. The *expected SHA-256* field is optional: if you fill it in, the file must match.
6. *Upload and install*. The page checks the file (header, VesevOS marker, size) before sending it; the board checks it again before activating it.
7. After about 10 seconds VesevOS restarts with the new version. To go back without updating: *Start VesevOS without updating*.

From the serial port you can also type `recovery` (says whether it is there) and `recovery now` (restarts into the recovery). In the recovery: `i` info, `b` start VesevOS, `r` restart.

## If something goes wrong
| Problem | Fix |
|---|---|
| *no sync reply* or port not found | Charge-only cable? Try another cable. Close the serial monitor. Enter download mode with BOOT + RESET (step 3.4). |
| The recovery is too big | The limit is 1,048,576 bytes (step 1): use the build configuration of the `idf-recovery` project. |
| The recovery page does not open | Private window and `http://IP:80/` (browsers often force https). Check the IP on the serial monitor. |
| *Not a VesevOS firmware (marker missing)* | Wrong file (for example the recovery's, or `.merged.bin`). You need `VesevOS-<version>.bin` from a version that carries the marker (1.7.44 or newer). |
| VesevOS does not start after an error | Normal: the recovery stays on. Upload the good file from its page. |
| Lost the administrator password | BOOT button 8 seconds (manual, chapter 2). In the recovery, if there is no administrator, you need the 8-digit code shown on the serial port. |
| I want to go back | `python3 -m esptool --chip esp32s3 -p PORT -b 460800 write_flash 0 ~/vesevos-backup-4MB.bin` |

## Security: good to know
- The recovery uses **HTTP only** (no encryption): the password never travels in clear (challenge-response sign-in), but the session code and the file can be seen on the local network. Use it on a network you trust.
- The file check verifies it is a complete VesevOS firmware; it is **not a digital signature**. The SHA-256 tells you the file is not corrupted, not who made it; the word "official" in the list is just a label. Download files only from this repository and compare the SHA-256.
- Not legal advice. See [SECURITY.md](../SECURITY.md) to report security problems.
