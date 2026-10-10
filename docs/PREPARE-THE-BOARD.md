# Preparing the board, step by step (VesevOS 1.7.45)

This guide takes a new (or reset) **ESP32-S3 SuperMini** to a working VesevOS with cable-free updates.
Time: about 40 minutes the first time. Examples are for macOS; on Linux and Windows only the port name changes (see step 5).
[Versione italiana](GUIDA-PREPARARE-LA-SCHEDA.md)

**How it works, in short.** Two programs live on the board: the **recovery** (small, used only to update) and **VesevOS**.
The first install is done over USB. After that VesevOS updates itself from the web page (user menu > *Update firmware*),
with no cable and no SD card.

## What you need
- The ESP32-S3 SuperMini board (4 MB flash, 2 MB PSRAM) and a USB cable **that carries data** (not charge-only).
- A computer with Arduino IDE 2.x and Python 3.
- The VesevOS repository downloaded from GitHub (*Code > Download ZIP*) and unzipped.

## Step 1 - Arduino IDE and libraries
1. Install **Arduino IDE 2.x** from arduino.cc.
2. *Preferences > Additional boards manager URLs*: add `https://espressif.github.io/arduino-esp32/package_esp32_index.json`.
3. *Tools > Board > Boards manager*: search **esp32** (by Espressif) and install version **3.3.x** or newer.
4. *Tools > Manage libraries*: install **PsychicHttp** (3.1.x), **ArduinoJson** (7.x) and **NimBLE-Arduino** (2.x, by h2zero).

## Step 2 - Board settings
In *Tools* choose, **for both the recovery and VesevOS**:

| Item | Value |
|---|---|
| Board | ESP32S3 Dev Module |
| PSRAM | QSPI PSRAM |
| USB CDC On Boot | Enabled |
| Partition Scheme | Huge APP (3MB No OTA/1MB SPIFFS) |

The partition scheme only lets the compiler check the size: the real one is written by the script in step 6.

## Step 3 - Build the recovery
1. Open `recovery/VesevOS_Recovery/VesevOS_Recovery.ino`.
2. *Sketch > Export compiled binary*. The folder `build/esp32.esp32.esp32s3/` appears next to the sketch.
3. `VesevOS_Recovery.ino.bin` must be **under 1,048,576 bytes** (about 1,010,000: it fits, with little margin).

## Step 4 - Build VesevOS
1. Open `VesevOS/VesevOS.ino` (the folder must also contain `src/`).
2. *Sketch > Export compiled binary*.
3. `VesevOS.ino.bin` must be **under 2,424,832 bytes** (about 2,026,000).

**Never use the IDE's *Upload* button** for VesevOS: it would write to the wrong place and erase the recovery.

## Step 5 - Upload tool (esptool)
1. In the terminal: `python3 -m pip install esptool`
2. Plug in the board and find the port: on macOS `ls /dev/cu.usbmodem*` (e.g. `/dev/cu.usbmodem14401`); on Linux `/dev/ttyACM0`; on Windows `COM5` (Device Manager).
3. Close the IDE's *Serial Monitor* (it holds the port).
4. If the board does not answer: hold **BOOT**, press and release **RESET**, release BOOT, and try again.

## Step 6 - Backup (recommended) and install
**Flash backup** (about 8 minutes, once; lets you go back to how it was):
`python3 -m esptool --chip esp32s3 -p PORT -b 460800 read_flash 0 0x400000 ~/vesevos-backup-4MB.bin`

**Install.** In the repository's `recovery/` folder:

- **New board, or start from scratch** (ERASES everything, including the configuration):
  `./installa.sh PORT RECOVERY_BUILD_FOLDER VESEVOS_BUILD_FOLDER/VesevOS.ino.bin`
- **Board already running VesevOS** (keeps the configuration):
  `./carica-tutto.sh PORT RECOVERY_BUILD_FOLDER VESEVOS_BUILD_FOLDER/VesevOS.ino.bin`

Example (macOS):
`./installa.sh /dev/cu.usbmodem14401 ~/Documents/Arduino/recovery/VesevOS_Recovery/build/esp32.esp32.esp32s3 ~/Documents/Arduino/VesevOS/build/esp32.esp32.esp32s3/VesevOS.ino.bin`

The scripts check that the files fit and tell you if something is missing. At the end they say "Fatto" (done).
If you see *"no sync reply"*: close the serial monitor, repeat step 5.4 and run it again.

## Step 7 - First start of VesevOS
1. Open the Serial Monitor (115200) and press RESET on the board. At start the recovery waits 3 seconds, then VesevOS starts.
2. Read the welcome screen: Wi-Fi network `VesevOS`, random password (12 characters) and address `http://192.168.4.1`.
3. Join that network with a phone or PC, open the page and **choose the panel password** (at least 6 characters).
4. Follow the **guided setup** (language, country, name, antenna, home Wi-Fi, time). Details: manual, chapter 1.

## Step 8 - Updating later (no cable)
1. Build and export the new version (step 4).
2. Open the VesevOS page, sign in as **Administrator**, user menu > **Update firmware** > *Restart into recovery*.
3. The page shows an address (for example `http://192.168.1.50:80/`). Open it in a **private browser window** and type **http**, not https.
   If within 30 seconds the board cannot find your home network, it creates the Wi-Fi network `VesevOS-recovery`: the password is on the serial monitor.
4. Sign in with the user and password of a VesevOS Administrator.
5. Choose `VesevOS.ino.bin`. The *expected SHA-256* field is optional: if you fill it in, the file must match.
6. *Upload and install*. The page checks the file (header, VesevOS marker, size) before sending it; the board checks it again before activating it.
7. After about 10 seconds VesevOS restarts with the new version. To go back without updating: *Start VesevOS without updating*.

From the serial port you can also type `recovery` (says whether it is there) and `recovery now` (restarts into the recovery). In the recovery: `i` info, `b` start VesevOS, `r` restart.

## If something goes wrong
| Problem | Fix |
|---|---|
| *no sync reply* or port not found | Charge-only cable? Try another cable. Close the serial monitor. Enter download mode with BOOT + RESET. |
| The recovery is too big | Use the same core (3.3.x) and the same settings as step 2. |
| The recovery page does not open | Private window and `http://IP:80/` (browsers often force https). Check the IP on the serial monitor. |
| *Not a VesevOS firmware (marker missing)* | Wrong file (for example the recovery's, or `.merged.bin`). You need `VesevOS.ino.bin` from 1.7.44 or newer. |
| VesevOS does not start after an error | Normal: the recovery stays on. Upload the good file from its page. |
| Lost the administrator password | BOOT button 8 seconds (manual, chapter 2). In the recovery, if there is no administrator, you need the 8-digit code shown on the serial port. |
| I want to go back | `python3 -m esptool --chip esp32s3 -p PORT -b 460800 write_flash 0 ~/vesevos-backup-4MB.bin` |

## Security: good to know
- The recovery uses **HTTP only** (no encryption): the password never travels in clear (challenge-response sign-in), but the session code and the file can be seen on the local network. Use it on a network you trust.
- The file check verifies it is a complete VesevOS firmware; it is **not a digital signature**. Download files only from trusted sources and, if you like, compare the SHA-256.
- Not legal advice. See [SECURITY.md](../SECURITY.md) to report security problems.
