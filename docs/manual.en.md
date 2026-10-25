# VesevOS 1.8.0b - Manual (chapters 1-26 describe 1.7.45-1.7.60; later changes are in chapter 27 and in the CHANGELOG)

Manual for people who use the board. Plain words, no programming.
Versione italiana: [manuale.md](manuale.md). The same help is in the board's page (Information > Help, or `http://192.168.4.1/#aiuto`).

> Not legal advice. The laws mentioned only explain why the board works this way.

## 1. First access to a new board

### Why the Wi-Fi password is random (and not written here)
The first time it starts, every VesevOS board makes up its own Wi-Fi password, different from all the others.

- **A password shared by everyone protects nobody.** If every board had the same password, reading this manual would be enough
  to get into any VesevOS board: your neighbour's, one in a shop, yours.
- **The law forbids it.** In Europe (Radio Equipment Directive 2014/53/EU with EN 18031, Cyber Resilience Act) and in the UK
  (PSTI Act) connected devices may not have initial passwords that are the same for everyone or easy to guess.
- **Only whoever holds the board knows it.** The password can be read only through the USB cable (or from the label you make).
- **It is not derived from the MAC or serial number**: those can be seen over the air, anyone could recompute it.
- **It is not in the network name**: everyone nearby sees the name, it would be like hanging the key on the door.
- **After setup you can change it** (Services > Access point), together with the network name.

### Method 1 (recommended): USB cable and serial monitor
1. Prepare the board with the guide [PREPARE-THE-BOARD.md](PREPARE-THE-BOARD.md) (recovery + VesevOS).
2. Open Tools > Serial Monitor at 115200 and press RESET on the board.
3. Read the welcome screen: network name (`VesevOS`), password (e.g. `k7mpq4xtr9ab`: 12 characters, lowercase letters and digits only, no symbols) and address `http://192.168.4.1`.
   Press `2` (no Enter) for English, `1` for Italian.
4. Join the `VesevOS` network with that password from a phone or PC. The page opens by itself; otherwise open `http://192.168.4.1`.
5. **Choose the panel password** (at least 6 characters). There is no factory password: you choose it and only you know it.
6. The **guided setup** starts (the LED shows a rainbow until you finish). Steps: language, country (list with search), board name, antenna,
   **home Wi-Fi or hotspot** (the board tries to connect at once and tells you), **time** (only without home Wi-Fi), finish. See chapter 12.
7. The **QR code** to connect your phone (and to print) is in **Network > Access point**, with name and password. Print the label and stick it under the board. The hotspot-password banner on the
   serial stays until the guide is finished (even if you reopen the monitor).

Any serial program works (PuTTY, screen, the "Serial USB Terminal" app with an OTG cable).
The QR code is not drawn on the serial (serial monitors often cannot show it): it is only in the Access point page. Setup can also be done from the serial line alone (chapter 13).

### Method 2: board prepared by someone else
Whoever prepares it reads the password as in Method 1 and hands over the board with the label (network name + password + QR).
The receiver chooses the panel password at first access and may change the Wi-Fi password too.

## 2. Lost passwords and the BOOT button

| BOOT button (hold, then release) | LED colour | What it does |
|---|---|---|
| under 2 seconds (1, 2 or 3 quick clicks) | - | the action chosen in Devices > BOOT button (factory: 1 click leaves airplane mode, 2 clicks start WPS) |
| 2 to 7 seconds | light blue | turns off the IP filter (if you locked yourself out) |
| 8 to 19 seconds | yellow | clears the administrators' passwords and prints the Wi-Fi password on the serial |
| 20 seconds or more | red | factory reset: erases everything, new Wi-Fi password |

- The action starts when you **release** the button: watch the colour and let go at the right moment.
- Wi-Fi password: panel > Network > Access point (name, password and QR). Or USB cable + BOOT 8 seconds.
- Panel password: the board keeps only a fingerprint, it cannot be read back. BOOT 8 seconds clears it; within 10 minutes
  you choose a new one (from the board's hotspot at any time).

## 3. Sign-in, users and roles
- Up to 8 users (Security > Users). **Administrator**: everything. **Operator**: uses the board (LED, pins, automations,
  messages) but does not change network, security or users; in the terminal only safe commands. **Guest**: views Home, status and log.
- The password never travels: the page sends only a computed proof (random number + fingerprint). After too many wrong
  passwords the same address is locked out for a while (Security > Password).
- The USB serial is password-protected (recommended to keep on).

## 4. HTTPS (encrypted page)
From the home network the page opens with `https://`. The certificate is made by the board and unique: the first time the
browser warns "connection not private". This is normal for home devices: compare the fingerprint shown by the browser with
the one in the page (Services > HTTPS) or on the serial, then continue. From the board's hotspot the page stays on `http://`
because the network is already encrypted by the Wi-Fi password. You can upload your own certificate (active after restart).

## 5. Country, radio and localization
System > Localization: country (list with search), language, time zone, time server, date and time formats,
decimal separator, first day of the week, C/F degrees, antenna and power.
- **The country sets the Wi-Fi channels and the maximum power** according to the rules the board knows: it never exceeds them.
  Without a country the safest rules apply (channels 1-11).
- The **Radio** page has two blocks: **Settings** (antenna, gain, power: you choose them) and **Information** (channels, country limit, maximum power, rules: calculated by the board, read-only).
- External antenna: enter its gain (dBi) and the power drops by itself to stay within the limit.
- **Choosing the country does not by itself guarantee that the finished product is compliant**: that also depends on the hardware, the antenna, the installation and the local rules. Whoever installs the board is responsible.

## 6. Services (off by default)
MQTT, board network, Bluetooth, time server for other devices and IP filter **are off**: you turn them on the first time.
- **MQTT** (Services > MQTT): sends the status to a broker and receives commands. With user and password use `mqtts` (encrypted).
- **Board network** (Services > Board network, ESP-NOW): boards talk without a router, up to 3 hops. All boards need the same
  shared key (64 digits); messages without the right key are dropped. Roles: node, gateway (forwards everything to MQTT), sensor.
  Content is not encrypted: do not send personal data.
- **Bluetooth** (same page): for phones and apps. Off by default; the page, the shell (`ble on [minutes]`) or an app (API `POST /api/ble`, optional `min`) turns it on. By default it stays on until switched off; the phone pairs with a random 6-digit code, different at each start and visible to the administrator only.

## 7. IP filter
Security > IP filter decides who can talk to the board (my network only, allow list, block list).
A new rule lasts 2 minutes: if the page still answers press **Confirm**, otherwise the previous rule comes back.
The board's hotspot and the serial are always allowed. Emergency exits: `firewall off` on the serial or BOOT 2-7 seconds.

## 8. Watchdog and checks
- **Watchdog** (Services > Watchdog): if a service hangs it restarts it, then the board. Optional: missing network, low RAM, scheduled restart.
- **Reduced mode** (1.7.11): after 3 automatic restarts in one hour the board **stops restarting and does not stay stuck**: if it knows which service hung (MQTT, board network, LED) it switches off only that one; if it does not (network, RAM, a basic service) it switches off MQTT, the board network and Bluetooth. The web page, log, update and recovery stay active. The Watchdog page says what was switched off and why. It lasts until the next restart; the saved configuration does not change.
- **Compliance status** (Security > Compliance status): the board checks its configuration. Red = forbidden value (law or security), fixed at once.
  Yellow = allowed but risky. The log says who changed what (page, serial, file).

## 9. Data and privacy
The board keeps: configuration, users (password fingerprint only), a 150-line log with the IP addresses of those who sign in,
alarms and recent changes. Nothing leaves the board unless you turn on MQTT or the board network.
To erase everything: Config > Reset everything, the `factory-reset` command or BOOT 20 seconds.

## 10. Security and updates
Security problems: write to paolellad79@gmail.com (answer within 7 days). Security updates for at least
Free project: updates have no guaranteed dates. Details in [SECURITY.md](../SECURITY.md).

VesevOS is free software, free of charge, supplied outside any commercial activity and without warranty (GPL-3.0, sections 15 and 16).
References (informative, not legal advice): Cyber Resilience Act (EU 2024/2847, recitals 18-19: does not apply to free software outside
commercial activity); Product Liability Directive (EU 2024/2853, Art. 2(2)); Radio Equipment Directive 2014/53/EU with Delegated
Regulation 2022/30 (EN 18031); UK PSTI Act 2022; Italian Civil Code art. 1229. If VesevOS becomes a paid product, these obligations
apply to that version and a support period will be declared.

## 11. Terminal (serial and page)
Type `help`. New commands: `welcome`, `ap`, `user`, `passwd`, `locale`, `firewall`, `audit`, `watchdog`, `mesh`, `ble`, `cert`, `legal`.
On the serial, at an empty line, keys `1` and `2` change language.

## 12. Menus, hotspot, HTTP/HTTPS, ports and end of the guide (new in 1.7.3, 1.7.4, 1.7.5, 1.7.6 and 1.7.7)
**Where things are**
- **Home**: rings (CPU with speed and temperature, RAM, PSRAM, files, Wi-Fi), a second row with one icon widget for each running service (the IP address is under the Wi-Fi widget; time and date are in the header; the Location shows when you click the flag) (HTTP/S, MQTT, ESP-NOW, mDNS, Bluetooth, NTP, DHCP, hotspot, power saving, statistics); click opens the service page. **Network**: Wi-Fi, IP address, name, airplane mode.
- **Services**: Access point (AP), HTTP, HTTPS, MQTT, Board network, Automations, Tasks, Watchdog, Boot, Terminal.
- **Peripherals** (was Hardware): Pins (front/back drawing and inventory), LED.
- **System**: Status, Files, Time, Localization (language too), Log, Config, Accessibility, Legal notes, Help.
- **Security**: Password (serial password too), Users, IP filter, Compliance status.

**Service controls (the same everywhere)**
- At the top there is the **status banner**: **Started** (green) or **Stopped** (red). Below, four icons with a coloured background for the state: **Start** (green), **Stop** (red), **Restart** (yellow, grey when the service is stopped or cannot restart), **Apply** (blue, HTTP and HTTPS only).
- HTTP and HTTPS: changes (on, off, ports) take effect after **Apply**, which restarts the board (with confirmation).
- Applies to: Access point, DHCP, DNS (captive portal), mDNS, HTTP, HTTPS, MQTT, Board network, Statistics, NTP, Bluetooth, Power saving, MFA.
- **Tasks** (System > Diagnostics): each task has the same Start / Stop / Restart icons; Restart is grey when the task is stopped; protected tasks show a lock.

**The user always chooses.** Every service can be turned on and off. Three guard rails remain: (1) you cannot lock yourself out: at least one
web protocol (HTTP or HTTPS) stays on, and BOOT 8 seconds restores hotspot, HTTP, HTTPS and ports to factory values; (2) the country's radio
rules cannot be exceeded; (3) no identical passwords.
- **Access point (AP)**: the board's own Wi-Fi. It turns on by itself at first start and when your Wi-Fi is not configured. It can be
  turned off only while the board is connected to your Wi-Fi. If the Wi-Fi disappears and the AP is off the board cannot be seen: recover
  with BOOT 8 seconds or from the serial line. The **automatic portal** (the phone opens the page by itself) works only with HTTP on port 80.
- **HTTP**: not encrypted. It serves the hotspot. While connected to your Wi-Fi you may turn it off: recommended, saves memory.
- **HTTPS**: encrypted (recommended). You may also turn it off and use HTTP only: your choice, everything works, but session and data are
  not encrypted (login never sends the password) and Compliance status shows a yellow warning.
- **Ports**: HTTP 80 or 1024-65535, HTTPS 443 or 1024-65535, never the same. **They apply after a restart** (the page tells you). Changing the port
  is not real security: security stays password, IP filter and encryption. Links and QR codes show the port.

**End of the guided setup**
- **Home Wi-Fi or hotspot** step: pick the network and password, press Next: the board tries to connect at once (up to 20 seconds) and tells you if it worked.
- **Time** step (only if you skip the Wi-Fi): the NTP service turns off and you are asked for time zone, date and time (preset from this device). With home Wi-Fi the time comes from the Internet (NTP) and this step does not appear. Note: **the board has no clock battery**, if you power it off the time is lost.
- At the end, with the home Wi-Fi, the page shows the new address (with QR). Connect your phone to that network and open it: **at the first
  login from there** hotspot, automatic portal and HTTP turn off by themselves. If the board cannot connect, the hotspot comes back by itself
  and a warning appears on Home. Without home Wi-Fi nothing turns off (except NTP).
- **Login page**: shows the board's date, time and time zone and warns if the time differs from your device.

## 13. Setup from the serial line only
Without the web page: open the serial monitor (115200) and type `setup`. It asks: language, country, name, antenna, hotspot (shows the
password, `n` = new), administrator password (visible while typing: do it somewhere safe), home Wi-Fi (numbered list), time.
Enter = keep the value, `skip` = skip, `quit` = exit. Single commands: `lang`, `region country XX` (`locale` stays valid), `hostname`, `wifi set <network> [password]` (with spaces: `wifi set "My home" password`),
`wifi off`, `ntp on|off`, `ntp server <name>`, `ntp server2 <name|off>`, `date set YYYY-MM-DD HH:MM`, `svc ap|captive|http|https on|off`, `svc http-port N`, `svc https-port N`, `ap new`.
After the guide the security rules apply (serial password, if on).

## 14. Network services: NTP, DHCP, DNS, mDNS (new in 1.7.3)

Each one has its own tab under **Services** and can be turned off.
- **NTP**: time from the Internet (needs the home Wi-Fi). Off = set time by hand. By default it contacts only `pool.ntp.org`; you may add a second server (`ntp server2 <name>`). Every request shows your public IP to the servers. If the received time is not credible (before 2026 or a jump of more than a day) it is discarded and a warning appears.
- **DHCP**: gives addresses to whoever joins the hotspot. Lease 10 to 1440 minutes. It can be turned off only while the home Wi-Fi is connected.
- **DNS**: the hotspot's automatic portal (needs HTTP on port 80).
- **mDNS**: the NAME.local name. Off = IP address only.
DHCP, mDNS and portal change at once; web and ports after reboot. Over serial: `svc dhcp off`, `svc mdns on`, `diag`.

If the guide appears at every start: press **The board is already configured** (or `setup done` over serial). "Later" does not mark it as finished.

## 15. Pins: drawing and inventory
Peripherals > Pins: **Front** drawing (edge pins, USB-C at the top, BOOT/RESET, RGB LED, antenna) and **Back** (solder pads GP14-18, 21, 33-42,
45-48 and pads B+, B-, BOOST: BOOST only with a battery over 500 mAh). Tap a pin to see functions and warnings (boot pins 0/3/45/46, ADC2 with
Wi-Fi on, USB 19/20, JTAG 39-42). **Inventory**: write what you connect to each pin (e.g. "door sensor"): the name stays in the configuration and
a warning appears if you test a pin already marked. GP33-37 are free with this board's 2 MB PSRAM (taken on boards with octal PSRAM).


## 16. Security and energy (new in 1.7.3)
- **MFA**: Security > Two-step login. Scan the QR with an app (Google Authenticator, Aegis...), type the code, keep the 8 recovery codes (each works once). The QR shows only over HTTPS or from the hotspot.
- **Time**: the code depends on time. Without valid time you can choose: ask the browser time (after correct password and code) or recovery codes only / block.
- **Recovery**: BOOT 8 s turns off MFA for Admins.
- **Anti-bot**: at login the browser solves a small proof of work (level in Security). No external captcha.
- **Certificate**: System > Certificate: expiry, fingerprint, download, regenerate, import.
- **Power saving**: Services > Power saving. Scheduled sleep = the board sleeps and wakes after the chosen time (1 min - 7 days). BOOT does not wake it: use RESET. While asleep Wi-Fi and page are off.
- **Local statistics**: off by default; counters that stay in the board and are never sent out; CSV download; reset.
- **Serial**: `power`, `sleep [min]`, `stats`.


## 17. Self-test (new in 1.7.7)
Administration > Self-test (Admin only). Press **Start diagnosis**: 16 tests one at a time, each with result OK / Warning / Error / Skipped. At the end **Download report** (or **Copy**). No passwords, keys or tokens; MAC, Wi-Fi name, IP and log are hidden unless "real names" is ticked. "Active tests" blinks the LED and sends a test MQTT message. Serial: `selftest [active] [names]`. **Close and free memory** deletes the report.


## 18. Log and reboot journal (new in 1.7.8)
System > Log: each line has the level [E] error, [W] warning, [I] info, [D] detail. **Show up to** only filters the view; **Record up to** (Admin) decides what is written (factory: Info; Detail only to hunt a problem). Below, the **Reboot journal**: last 20 boots with reason, how long the board was on, minimum RAM and busiest task; it survives power-off. Serial: `log level [0-3]`, `reboots [clear]`. The journal is in the Self-test report (the last serious line only with "real names").

## 19. Configurable serial port (new in 1.7.9)
System > Security > **Serial port (USB cable)** (Admin only) or, from the serial port itself, the `serial` command. Changes apply at once and stay after a restart.
- **Speed (baud)**: 9600 ... 921600 (factory 115200). It only matters on a UART serial port; the ESP32-S3 native USB does not use it. A new speed is **on trial for 20 seconds**: if you do not confirm it (**Confirm** button or `serial keep`) the old one comes back, so you cannot lock yourself out.
- **Line ending**: CR+LF (factory, right for PuTTY and Windows), LF only, CR only.
- **Key echo**, **The serial port accepts commands**, **Write the log to the serial port**, **Show the welcome screen when you open the monitor**: switches.
- **Maximum write wait** (0-200 ms, factory 10): how long the board waits for the PC before dropping text. With 0 it may lose pieces of text.
- Commands: `serial` (show), `serial baud 115200`, `serial eol crlf|lf|cr`, `serial echo|input|log|banner on|off`, `serial tx 10`, `serial keep`. Turning commands off needs `serial input off yes`.
- If you turned "accepts commands" off and need the serial port: turn it back on from this page, or hold BOOT for 8 to 19 seconds (this also resets the administrator passwords).
- Serial (1.7.9a): Backspace deletes the last character (on screen too), Ctrl+U or Ctrl+C clear the line; arrow keys and Delete are ignored.
- `ram` (1.7.9a, measuring to reduce RAM): for each task the minimum free stack, the total stack and how much could be removed. `ram mark` marks the RAM now; turn a service on or off and type `ram diff` to see what it costs (internal RAM, largest piece, PSRAM). Admin only.
- `free detail` (1.7.9a): internal RAM, largest piece, fragmentation, PSRAM. If the largest piece drops below 45 KB a yellow "Memory is fragmented" warning appears: restart the board.

## 20. Device Manager (new in 1.7.11)
One menu group, **Device Manager**, with all peripherals: virtual (MQTT, board network, Bluetooth, MFA, statistics) and hardware (pins, LED, BOOT button, USB, temperature, memory, processor, radio). Each follows the same rules: list, state (off / on / with a problem), switch on and off (Admin only), details and actions.
- From the serial port or the terminal: `dev list`, `dev status <name>`, `dev on <name>`, `dev off <name>`, `dev act <name> <action> [value]` (e.g. `dev act pin test 5,high`).
- **Build-time packages** (for people who compile): in `vos_common.h` `VOS_WITH_BLE`, `VOS_WITH_MQTT`, `VOS_WITH_MESH`, `VOS_WITH_MFA`, `VOS_WITH_STATS` (1 = in the firmware, 0 = out). The pages of removed parts disappear. The `diag` command lists the packages present.

## 21. Backup and restore (new in 1.7.11)
System > **Backup and restore** (Admin only).
- **Download without secrets**: configuration without passwords and keys (default).
- **Include the secrets too**: tick it, choose a **phrase of at least 10 characters**, type it twice and accept the warning. The file is **encrypted** (PBKDF2-HMAC-SHA256 + AES-256-GCM): it cannot be read without the phrase. Whoever has the file and the phrase can get into the board and your network; **if you lose the phrase the secrets cannot be recovered**. Every export with secrets is written in the log.
- **Restore from file**: choose the file (and, if it is encrypted, type the phrase). The page tells how many lines changed. A wrong phrase counts as a login failure.
- **Factory reset**: erases settings, users and passwords; to confirm you must type the word ERASE.

## 22. Updating the firmware with no cable (new in 1.7.42 - 1.7.45)
The board holds two programs: **VesevOS** and the **recovery** (small, used only to update). The first install is done over USB: follow [PREPARE-THE-BOARD.md](PREPARE-THE-BOARD.md). After that you update from the page.
1. User menu (Administrator only) > **Update firmware** > *Restart into recovery*. Or, from the serial port: `recovery` (says whether it is there) and `recovery now`.
2. The page waits for the board to restart and **opens the recovery by itself** (usually within seconds; "Open now" to skip the wait). If it cannot, it shows the address, for example `http://192.168.1.50:80/`: open it in a **private window** and type **http**, not https (the recovery is not encrypted).
   The recovery tries your **home network** first; if it cannot find it within 30 seconds it creates the Wi-Fi network `VesevOS-recovery` (random password written on the serial port).
3. Sign in with the user and password of a VesevOS **Administrator** (the password never travels in clear). With an empty flash and no administrators you need the 8-digit code shown on the serial port. Five mistakes = a 60-second wait.
4. Choose the firmware `.bin` file (not `.merged.bin`). *Expected SHA-256* is optional: if you fill it in, the file must match.
5. Pick the file with the button or **drag it** onto the page (on a phone, button only). *Upload and install*. First the page and then the board check: ESP32-S3 header, **VesevOS marker**, size, complete image. A wrong file is refused and **the old firmware is left untouched**; if the error comes after writing has started, the recovery stays on and you just upload the good file.
6. The board **LED** shows the state: slow orange = waiting for the file; fast cyan = writing; solid green = done; red = error (back to orange after 10 seconds). After about 10 seconds VesevOS restarts. *Start VesevOS without updating* goes back. After 15 minutes without access the recovery goes back to VesevOS by itself. BOOT held at power-up = stay in the recovery.
- The file check guarantees it is a complete VesevOS firmware; it is **not a digital signature**. Download files only from trusted sources.
- After installing with the recovery **do not use the IDE's *Upload***: it would erase the recovery.

## 23. Device classes: relay, LED, pin, input (new in 1.7.54)
- **Where**: Devices > Periferiche (Devices). Choose the *class* (relay, LED, output pin, input, analog), the *pin* and, if you like, a *name*: with no name the board calls them rele1, led1, pin1, in1, adc1. Names are lowercase letters without accents and digits (2-11 characters); change them with *Rename*. Maximum 16 devices. They are stored in `/io.cfg` (not in the configuration backup), with a copy of the last good version in `/io.cfg.bak`. If the file storage is full the board answers "Spazio esaurito" (no space left) and the list does not change. A device not loaded at boot (pin in use) stays in the file and comes back when the pin is free.
- **Checked pins**: the board refuses USB (19, 20), BOOT (0), RGB LED (48), boot pins (3, 45, 46), pins that do not exist or are already in use. Analog works only on pins 1-10. (SuperMini numbers: they come from the firmware board file, and the page reads them from there.)
- **Active low**: for relay modules that switch on with level 0.
- **Safety**: after every restart, after a crash or in the watchdog reduced mode all outputs are **off**. The relay is switched only by the Administrator (operator and apps only if the Admin granted the permission) and changes at most once per second. A relay connected to mains power (230 V) needs a certified module and an installer: VesevOS only drives the signal.
- **Commands**: page; shell `io list`, `io add relay 5 [name] [inv]` (`rele` stays valid), `io del <name>`, `io rename <name> <new>`, `io set <name> 1`, `io get <name>`; `dev on|off <name>` and `dev act <name> set 1`.
- **In apps**: write the name in `use=` (for example `use=rele1,in1`); then `dev_set("rele1", 1)` and `dev_get("in1")` (input: 0/1, analog: 0-4095). Without the name in `use=` the app stops with "permission missing".
- **Device list**: the devices you create appear under *Periferiche*.

## 24. Module cards and a page for each device (new in 1.7.55)
- **Page for each device**: in Devices > Periferiche tap *Manage*. You see class, pin, level, live value and the **tests**: Switch on, Switch off, 1-second pulse, Blink for 5 seconds (LED and pin only; the relay does not blink). Inputs and analog devices show a value that updates by itself. *Rename* and *Remove* (Admin) are here too. Pulse and blinking switch off by themselves; *switching off* is always possible, at once.
- **Module card (.vdp)**: a text file describing a module (class, signals, icon, SVG diagram). Data only. Folder `/devs`. Examples are in `docs/devs` (1-, 2- and 4-channel relay, LED, button, PIR sensor, LDR).
- **How to use**: Periferiche > *Upload module card*, then *Install*: the board shows the diagram, suggests free and safe pins, and creates the devices (all or nothing). You can choose the name (pompa1, pompa2...).
- **Wiring diagram (new in 1.7.57)**: the page draws it from the card data. The board with its pins (5V or 3V3, GND, suggested GPIOs) on the left, the module on the right. Coloured wires: **red** supply, **black** ground, **blue** signal (orange if analog), with the **voltage written** on the wire and an arrow for the direction. Resistors and other parts to add are drawn with their value (e.g. 330 ohm in series with the LED). Below is a table wire / from the board / to the module / voltage and notes: colours are also written in words. 12, 24 or 230 V supplies show a "Power supply" box: join its ground to the board GND. If the module signal is 5 V on an input a red warning appears: the pins only tolerate 3.3 V.
- **For card authors**: add `level=3V3|5V` (signal level) and up to 4 lines `part=serie|gnd|vcc,value,signal` (for example `part=serie,330 ohm,OUT`) to `[module]`. The `[wiring]` section is no longer needed: if present it shows below as "Drawing attached to the file". Full format: in the internal documentation of the .vdp format.
- **Serial**: `mod list`, `mod info <name>`, `mod install <name> [prefix]`, `mod del <name>`.
- **Warning**: always check the connections on your module's datasheet. A wrong diagram can damage the module. For 230 V you need an installer.

## 25. WPS: join the router without typing the password (new in 1.7.56)
- **What it is for**: the router sends the board the name and password of your home Wi-Fi. You do not type them. The board saves them and connects.
- **Where**: Rete (Network) > **WPS** (Administrator only). At the top there is the switch (Start/Stop): **off** from the factory. The first-setup guide has a *Connect with WPS* button (it also works with the switch off until the guide is finished).
- **Button method**: press *Connect with the router button*, then within 2 minutes press the WPS button on the router.
- **PIN method** (off from the factory, turn it on in the WPS settings): the board creates a new 8-digit PIN at every try and shows it; you enter it on the router's WPS page within 2 minutes. The PIN is never written to the log.
- **From the BOOT button**: by default **2 quick clicks** start WPS and 2 clicks again cancel it (WPS must be on, or the guide not finished). 1 click leaves airplane mode. The actions for 1, 2 and 3 clicks are chosen in *Devices > BOOT button* (free for scripts, leave airplane mode, WPS, find the board); the board waits half a second after the last click. A gesture set to **Free** does nothing by itself: if a script (or an app with `btn` in `use=`) is running, it reads those clicks with `btn()` (1, 2 or 3; 0 if there are none; it does not wait, so use it in a loop with `sleep`). With no script running the free clicks are lost. The other times do not change (2 s IP filter, 8 s password, 20 s factory). The same page has a **Button test**: for 60 seconds the button does nothing (not even the long presses) and the page shows what the board understands. Note: BOOT held while the board powers up enters download mode (this is not WPS).
- **LED**: slow breathing blue = waiting, solid green for 3 s = done, solid red for 3 s = error.
- **What happens**: during the try the hotspot stays on. On success the name and password replace the old ones, the address becomes automatic (DHCP) and the board joins the new network; if you were on the hotspot the page drops, reconnect your phone to the home network. On failure the board goes back as it was. A line is left in the checks log ("The home Wi-Fi was changed with WPS").
- **Limits**: it does not work with WPA3-only routers or with WPS turned off on the router; two routers in WPS at once can be confused; 2.4 GHz only. It starts only with Wi-Fi on (not in airplane mode).
- **Serial**: `wps` (status), `wps on|off`, `wps pin on|off`, `wps button on|off`, `wps start [pin]`, `wps stop` (Administrator only).
- **Safety**: with WPS on and mapped to a button gesture, whoever holds the board can connect it to their own network: if the board is not safe, remove the gesture or turn WPS off. WPS is a trademark of the Wi-Fi Alliance.

## 26. Scripts and apps: reading the log and writing to the MQTT broker (new in 1.7.60)
- **`logtail(n)`**: returns the last `n` lines of the log (1 to 20, 10 by default), at most 2000 characters. Always available in scripts; in apps it needs `logread` in `use=`.
- **`mqtt("name", "text")`**: sends the text to the broker on the topic `<prefix>/app/<name>`. Returns true if it was sent, false if it was not sent (MQTT not connected or over the limit). The name has 1 to 40 characters (a-z, 0-9, `_`, `-`, `/`), the text up to 2048. At most **20 messages per minute**. In apps it needs `mqtt` in `use=`.
- **Where it goes**: only under `app/`. An app cannot write to `cmd` (the board commands), `status` or `state`.
- **MQTT must be on** (Network > MQTT, off by default) and you need a broker. Forwarding to email is done by a program of your own (for example Home Assistant, Node-RED, ntfy): the board does not talk to mail servers.
- **Example**: `docs/esempi/app/logmqtt.vap` (copy it to /apps): a button and an "every hour" switch that send the last 10 log lines to `<prefix>/app/log`.
- **Flashlight example**: `docs/esempi/script/torcia.vsc` (copy it to /scripts and start it). One click of the BOOT button turns the white LED on, the next click turns it off; it also turns off by itself after 10 minutes (change `MINUTI`, 0 = never). First set **Free** on the gesture you want in *Devices > BOOT button* (by default only 3 clicks are free). The white LED is bright and gets warm.
- **Privacy**: the log can contain IP addresses and names. Whoever installs the app (Administrator only) decides with `logread` and `mqtt` whether it may read it and send it out. If the broker is outside your home, use encrypted MQTT (user and password) and choose who receives the data.

## 27. What is new in 1.8.0 and 1.8.0b (summary)
- **Serial login**: the shell asks for user, password and MFA code; roles for each subcommand; the password stays out of the history. After too many errors the same lock as on the web page applies.
- **Commands**: `pwd`, `cd`, `ls`, prompt `user@hostname:folder$`, Tab, `man <command>`, `whoami`, `id`, `df`, `uname`, `time`, `ping`, `gzip`/`gunzip`/`zcat`, `zip`/`unzip`, `tar`, `cpu` (governor: 80/160/240/ondemand/conservative/schedutil), and the `vesevos-*` commands (cpuinfo, services, bugreport, config, network, led_control, cleaner, drive_manager, backup, update).
- **Security**: `suspend`, `poweroff` and `/api/sleep` for Admin only, with confirmation. `locale country/tx/antenna`, `wifi set/off`, `date set` and `audit ack` are forbidden to the Operator.
- **LED**: triggers `none/state/heartbeat/flash/wifi/cpu/default-on`, colors and CPU threshold.
- **Network**: with the HTTP->HTTPS redirect on and HTTPS active, the HTTP port is served by a small server that only answers the redirect (less RAM; takes effect when the servers restart).
- **Updates**: *Update via recovery* menu (Admin only); ready files, SHA-256 and `indice.json` in the repository's `firmware/` folder; first install from the browser with the `flash/` page (NOT TESTED with a real board). There is no digital signature: SHA-256 tells you the file is not corrupted, not who made it.
- This is a summary: details and the tests still to be done are in the CHANGELOG and in `docs/KNOWN_ISSUES.en.md`.
