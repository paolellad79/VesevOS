# VesevOS 1.7.2 - Manual

Manual for people who use the board. Plain words, no programming.
Versione italiana: [manuale.md](manuale.md). The same help is in the board's page (System > Help, or `http://192.168.4.1/#aiuto`).

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
1. Upload VesevOS to the board (Arduino IDE > Upload).
2. Open Tools > Serial Monitor at 115200 and press RESET on the board.
3. Read the welcome screen: network name (`VesevOS`), password (e.g. `k7Hm-pQ4x-Tr9a`) and address `http://192.168.4.1`.
   Press `2` (no Enter) for English, `1` for Italian.
4. Join the `VesevOS` network with that password from a phone or PC. The page opens by itself; otherwise open `http://192.168.4.1`.
5. **Choose the panel password** (at least 6 characters). There is no factory password: you choose it and only you know it.
6. The **guided setup** starts (the LED shows a rainbow until you finish). Steps: language, country on the map, board name, antenna,
   hotspot password, **home Wi-Fi** (optional), **time**, finish. See chapter 12.
7. The **QR code** to connect your phone (and to print) is on the **Home** page: in hotspot mode it is the board's Wi-Fi QR, when the board
   is on your Wi-Fi it is the QR with the board's address. Print the label and stick it under the board. The hotspot-password banner on the
   serial stays until the guide is finished (even if you reopen the monitor).

Any serial program works without Arduino IDE (PuTTY, screen, the "Serial USB Terminal" app with an OTG cable).
The QR code is not drawn on the serial (serial monitors often cannot show it): it is only in the Home page. Setup can also be done from the serial line alone (chapter 13).

### Method 2: board prepared by someone else
Whoever prepares it reads the password as in Method 1 and hands over the board with the label (network name + password + QR).
The receiver chooses the panel password at first access and may change the Wi-Fi password too.

## 2. Lost passwords and the BOOT button

| BOOT button (hold, then release) | LED colour | What it does |
|---|---|---|
| under 2 seconds | - | leaves airplane mode |
| 2 to 7 seconds | light blue | turns off the IP filter (if you locked yourself out) |
| 8 to 19 seconds | yellow | clears the administrators' passwords and prints the Wi-Fi password on the serial |
| 20 seconds or more | red | factory reset: erases everything, new Wi-Fi password |

- The action starts when you **release** the button: watch the colour and let go at the right moment.
- Wi-Fi password: panel > Home (QR, hotspot mode only) or Services > Access point > Show. Or USB cable + BOOT 8 seconds.
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
System > Localization: country (zoomable world map), language, time zone, time server, date and time formats,
decimal separator, first day of the week, C/F degrees, antenna and power.
- **The country sets the Wi-Fi channels and the maximum power** according to its rules: the board never exceeds them.
  Without a country the safest rules apply (channels 1-11).
- External antenna: enter its gain (dBi) and the power drops by itself to stay within the limit. Fitting it is the installer's responsibility.

## 6. Services (off by default)
MQTT, board network, Bluetooth, time server for other devices and IP filter **are off**: you turn them on the first time.
- **MQTT** (Services > MQTT): sends the status to a broker and receives commands. With user and password use `mqtts` (encrypted).
- **Board network** (Services > Board network, ESP-NOW): boards talk without a router, up to 3 hops. All boards need the same
  shared key (64 digits); messages without the right key are dropped. Roles: node, gateway (forwards everything to MQTT), sensor.
  Content is not encrypted: do not send personal data.
- **Bluetooth** (same page): only to configure from a phone, turned on by hand for 10 minutes with a pairing code.

## 7. IP filter
Security > IP filter decides who can talk to the board (my network only, allow list, block list).
A new rule lasts 2 minutes: if the page still answers press **Confirm**, otherwise the previous rule comes back.
The board's hotspot and the serial are always allowed. Emergency exits: `firewall off` on the serial or BOOT 2-7 seconds.

## 8. Watchdog and checks
- **Watchdog** (Services > Watchdog): if a service hangs it restarts it, then the board; at most 3 automatic restarts per hour,
  then it stops and warns. Optional: missing network, low RAM, scheduled restart.
- **Compliant** (Security > Compliant): the board checks its configuration. Red = forbidden value (law or security), fixed at once.
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

## 12. Menus, hotspot, HTTP/HTTPS, ports and end of the guide (new in 1.7.2)
**Where things are**
- **Home**: overview and QR. **Network**: Wi-Fi, IP address, name, airplane mode.
- **Services**: Access point (AP), HTTP, HTTPS, MQTT, Board network, Automations, Tasks, Watchdog, Boot, Terminal.
- **Peripherals** (was Hardware): Pins (front/back drawing and inventory), LED.
- **System**: Status, Files, Time, Localization (language too), Log, Config, Accessibility, Legal notes, Help.
- **Security**: Password (serial password too), Users, IP filter, Compliant.

**The user always chooses.** Every service can be turned on and off. Three guard rails remain: (1) you cannot lock yourself out: at least one
web protocol (HTTP or HTTPS) stays on, and BOOT 8 seconds restores hotspot, HTTP, HTTPS and ports to factory values; (2) the country's radio
rules cannot be exceeded; (3) no identical passwords.
- **Access point (AP)**: the board's own Wi-Fi. It turns on by itself at first start and when your Wi-Fi is not configured. It can be
  turned off only while the board is connected to your Wi-Fi. If the Wi-Fi disappears and the AP is off the board cannot be seen: recover
  with BOOT 8 seconds or from the serial line. The **automatic portal** (the phone opens the page by itself) works only with HTTP on port 80.
- **HTTP**: not encrypted. It serves the hotspot. While connected to your Wi-Fi you may turn it off: recommended, saves memory.
- **HTTPS**: encrypted (recommended). You may also turn it off and use HTTP only: your choice, everything works, but session and data are
  not encrypted (login never sends the password) and Compliant shows a yellow warning.
- **Ports**: HTTP 80 or 1024-65535, HTTPS 443 or 1024-65535, never the same. **They apply after a restart** (the page tells you). Changing the port
  is not real security: security stays password, IP filter and encryption. Links and QR codes show the port.

**End of the guided setup**
- **Home Wi-Fi** step: search the network, type the password, or **Skip**.
- **Time** step: with the home Wi-Fi the time comes from the Internet (NTP). If you skip the Wi-Fi the NTP service turns off and asks for time
  and date (preset from this device). Note: **the board has no clock battery**, if you switch it off the time is lost.
- At the end, with the home Wi-Fi, the page shows the new address (with QR). Connect your phone to that network and open it: **at the first
  login from there** hotspot, automatic portal and HTTP turn off by themselves. If the board cannot connect, the hotspot comes back by itself
  and a warning appears on Home. Without home Wi-Fi nothing turns off (except NTP).
- **Login page**: shows the board's date, time and time zone and warns if the time differs from your device.

## 13. Setup from the serial line only
Without the web page: open the serial monitor (115200) and type `setup`. It asks: language, country, name, antenna, hotspot (shows the
password, `n` = new), administrator password (visible while typing: do it somewhere safe), home Wi-Fi (numbered list), time.
Enter = keep the value, `skip` = skip, `quit` = exit. Single commands: `lang`, `locale country XX`, `hostname`, `wifi set <network> [password]`,
`wifi off`, `ntp on|off`, `date set YYYY-MM-DD HH:MM`, `svc ap|captive|http|https on|off`, `svc http-port N`, `svc https-port N`, `ap new`.
After the guide the security rules apply (serial password, if on).

## 14. Pins: drawing and inventory
Peripherals > Pins: **Front** drawing (edge pins, USB-C at the top, BOOT/RESET, RGB LED, antenna) and **Back** (solder pads GP14-18, 21, 33-42,
45-48 and pads B+, B-, BOOST: BOOST only with a battery over 500 mAh). Tap a pin to see functions and warnings (boot pins 0/3/45/46, ADC2 with
Wi-Fi on, USB 19/20, JTAG 39-42). **Inventory**: write what you connect to each pin (e.g. "door sensor"): the name stays in the configuration and
a warning appears if you test a pin already marked. GP33-37 are free with this board's 2 MB PSRAM (taken on boards with octal PSRAM).
