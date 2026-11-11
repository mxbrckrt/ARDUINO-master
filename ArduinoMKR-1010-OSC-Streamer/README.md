# Arduino MKR WiFi 1010 — OSC Streamer, 3-Axis Accelerometer & Max/MSP

Low-latency wireless OSC sensor streamer for **Arduino MKR WiFi 1010** with a built-in **12-bit 3-axis accelerometer engine**, **Flash-stored Wi-Fi manager**, **embedded HTML monitor**, **Deep Sleep**, and a **100% native Max/MSP patch** (no third-party Max externals required).

Files in this project:
- [ArduinoMKR-1010-OSC-Streamer.ino](./ArduinoMKR-1010-OSC-Streamer.ino) — MKR WiFi 1010 firmware (OSC, Accelerometer DSP, Switch State Machine, HTTP UI, FlashStorage)
- [ArduinoMKR-1010-OSC-Streamer.maxpat](./ArduinoMKR-1010-OSC-Streamer.maxpat) — Native Max 9 dashboard & bidirectional OSC controller
- [README.md](./README.md) — Hardware & software documentation

---

## 1. Block Diagrams

### A. Hardware Wiring

```text
+-----------------------------------+        +------------------------------------------+
|     Power Supply (3.3V Logic)     |        |      Arduino MKR WiFi 1010 (SAMD21)      |
|  USB 5V or 3.7V LiPo (JST) -------+------->| 3.3V Regulated Rail (NOT 5V tolerant!)   |
+-----------------------------------+        |                                          |
                                             |   A0 <------ X  +----------------------+ |
+-----------------------------------+        |   A1 <------ Y  |  3-Axis Accel        | |
|  Momentary Switch (Unlatch)       |        |   A2 <------ Z  |  (SparkFun ADXL3xx)  | |
|   [Pin 1] ------------------------+------->|   D7 (INPUT_PULLUP)  VCC <- 3.3V       | |
|   [Pin 2] ------------------------+------->|   GND                GND <- GND        | |
+-----------------------------------+        |                                          |
                                             |   A3..A6   Optional analog inputs        |
                                             |   D2..D5,  Digital inputs (INPUT_PULLUP) |
                                             |   D8..D14  (D13/D14 = Serial1 RX/TX)     |
                                             |                                          |
                                             |   D6 ----> Onboard LED (Hardware PWM)    |
                                             |                                          |
                                             |  [FlashStorage]   [NINA-W102 Wi-Fi SPI]  |
                                             +--------------------------+---------------+
                                                                        |
                                                                2.4 GHz Wi-Fi
```

> **Hardware Safety Warning:** The MKR WiFi 1010 operates strictly at **3.3V**. Always power external sensors from the **`3.3V`** pin — never feed 5V into any analog or digital input.

---

### B. Signal Processing, Network & OSC Pipeline

```text
[12-bit ADC: 0..4095] (double-read per pin, configurable 1..7 active pins, default 3 = A0..A2)
   |
   +--> Clip -> Exponential Smooth -> Gamma ---> /analog/Ax, /analog-one/Ax, /analog-hundred/Ax
   |
   +--> 3-Axis Accel Engine (A0=X, A1=Y, A2=Z):
          |
          +--> Calibrated Tilt (Zoom * Gain) ----------> /accel/x, /accel/y, /accel/z
          |
          +--> High-Pass Dynamic Motion (gravity-free) -> /accel/dyn/x, /accel/dyn/y, /accel/dyn/z
                 |
                 +--> 3D Magnitude sqrt(dx^2+dy^2+dz^2) -> /accel/mag
                 |
                 +--> Slope Derivative (deadband 0.008) -> /accel/dir/x, /y, /z (+1 / 0 / -1)
                 |
                 +--> Bang Threshold (default 0.60,     -> /accel/trig/x, /y, /z, /mag (int 1)
                        70% hysteresis, 60ms cooldown)
                                                            |
                                                            v
                                        [sendOscPacket4: zero-malloc UDP 3033]
                                                            |
        +---------------------------------------------------+-----------------------------------+
        | (AP Mode: 192.168.1.128 -> 192.168.1.255:3033)                                        |
        v                                                                                       |
[Max/MSP: udpreceive 3033] ---> [route ...]                                                     |
        |                                                                                       |
        +--> /accel/trig/mag ---> [button] ---> [s -external-arpeggio-start-]                    |
        +--> /accel/dyn/* & /mag ---> 4x overlaid [multislider] oscilloscope                    |
        +--> [udpsend <MKR_IP> 3034] ---> handleIncomingOSC() (live Gain / Thresholds / Sleep)  |
                                                                                                |
[Web Browser: http://<MKR_IP>/] <---> HTTP Server (Port 80) <---> [FlashStorage] <--------------+
```

---

### C. Switch (`D7`) & Status LED (`D6`) State Machine

```text
                       +------------------------------+
  Power On / Reset --->|   Client Mode: Disconnected  | <--- Auto re-scan every 15s
                       |   LED (D6): Fast Blink       |
                       +------+----------------+------+
                              |                ^
        Known SSID found      |                | Wi-Fi lost (2 checks / 500ms)
                              v                |
                       +------------------------------+
                       |    Client Mode: Connected    |
                       |    LED (D6): Solid Full ON   |
                       +--------------+---------------+
                                      |
               D7: Short + Hold > 3s  |  D7: Short + Hold > 3s
                 (from either Client  |  or Web UI "Connect Client Mode"
                  or AP state)        v
                       +------------------------------+
                       |           AP Mode            |
                       | SSID: "Arduino AP mode"      |
                       | Pass: "12345678"             |
                       | IP:   192.168.1.128          |
                       | OSC:  192.168.1.255:3033     |
                       | LED (D6): Slow Pulse         |
                       +--------------+---------------+
                                      |
               D7: Direct Hold > 5s   |  (from any active state)
                                      v
                       +------------------------------+
                       |          Deep Sleep          |
                       |  NINA Wi-Fi in HW Reset      |
                       |  SAMD21 LowPower.deepSleep() |
                       |  LED (D6): OFF               |
                       +--------------+---------------+
                                      |
                       Press D7 (FALLING edge wakeup)
                                      v
                        Reboots Wi-Fi into last mode
```

---

## 2. Hardware Pinout

| Signal | MKR Pin | Mode / Notes |
|---|---|---|
| Accelerometer X | `A0` | 12-bit ADC (`0..4095`), powered from `3.3V` |
| Accelerometer Y | `A1` | 12-bit ADC (`0..4095`) |
| Accelerometer Z | `A2` | 12-bit ADC (`0..4095`) |
| Extra Analog Pins | `A3..A6` | Disabled by default (`activeAnalogCount = 3`) to eliminate floating-pin noise |
| Command Switch | `D7` | `INPUT_PULLUP` momentary unlatch switch wired between `D7` and `GND` |
| Status LED | `D6` | Onboard `LED_BUILTIN` driven via hardware PWM (`TCC0_CH6`) |
| Digital Inputs | `D2..D5, D8..D14` | `INPUT_PULLUP` (Note: `D13`/`D14` share `Serial1` RX/TX on MKR) |

---

## 3. Arduino Firmware Setup & Features

### Required Arduino Libraries
Install via **Arduino IDE -> Library Manager** (board package: **Arduino SAMD Boards** -> **Arduino MKR WiFi 1010**):
- `WiFiNINA`
- `OSC` (CNMAT)
- `ArduinoLowPower`
- `FlashStorage`

### Key Design Highlights
- **No plaintext Wi-Fi credentials in source code:** Up to **5 Wi-Fi networks** (`SSID`, passphrase, `OSC Target IP`, optional static `MKR IP`, `Gateway IP`) plus tuning parameters are stored in internal Flash via `FlashStorage`. *(Note: SAMD21 emulates EEPROM in program Flash, so settings persist across reboots and Deep Sleep, but are erased whenever you re-upload firmware via USB.)*
- **Zero-allocation OSC transmission (`sendOscPacket4`):** Builds OSC UDP packets on the stack without `malloc`/`free`, preventing SAMD21 heap fragmentation, with a `15 ms` backoff if NINA's TX queue fills up.
- **Clean AP <-> Client switching (`resetWiFiModule`):** Hardware-resets the NINA-W102 chip via `SpiDrv::end()` / `SpiDrv::begin(true)` when changing modes so TCP port `80` never gets stuck on stale AP/STA sockets.
- **Switch `D7` gestures:**
  - Press / Release: immediate `/switch/raw 1` / `0`
  - `1` short click: `/switch/short 1` (after `700 ms` multi-click window)
  - `2` quick clicks: `/switch/double 1`
  - `3` quick clicks: runs **Zero Calibration** + emits `/switch/triple 1` & `/accel/calibrated 1`
  - `1` short click + hold `> 3s`: toggles **AP Mode ON / OFF**
  - Direct hold `> 5s`: enters **Deep Sleep** (wake up by pressing `D7`)

---

## 4. Embedded HTML Monitor & Wi-Fi Manager

Open **`http://192.168.1.128/`** in AP Mode or **`http://<MKR_IP>/`** in Client Mode.

> **Browser Note:** Use `http://` explicitly (not `https://` — some browsers like Chrome auto-upgrade URLs to HTTPS, which fails on the local embedded HTTP server).

### UI Features
- **Trigger & Direction Badges:**
  - `TRIG X / Y / Z / MAG` badges flash for `220 ms` whenever a threshold bang fires.
  - Fixed-width `DIR X / Y / Z` badges (`▲` ascending, `▼` descending, `•` steady) sit on a dedicated row with zero layout jitter.
- **Color-Matched Bars & Live Plot:** `dynX` (Red `#ff453a`), `dynY` (Green `#30d158`), `dynZ` (Blue `#0a84ff`), `Mag` (White `#ffffff`). Uncheck **Live** during live performance to stop 10 Hz `/api/data` polling.
- **Settings Card:** Adjust Gain, Thresholds (`X/Y/Z`, `Mag`), Smoothing `k`, Active Analog Pins (`1..7`), **Apply** (RAM), **Save to Flash**, **Calibrate Zero**, and **Deep Sleep**.
- **Known Wi-Fi Networks Card:** Click **Scan 2.4GHz**, add/update or delete saved networks (passwords are never returned in `/api/wifi`), and switch immediately via **Connect Client Mode**.

### HTTP API Endpoints

| Endpoint | Method | Description |
|---|---|---|
| `/` | `GET` | Serves the HTML monitor, Wi-Fi manager, and built-in OSC reference |
| `/api/data` | `GET` | Live JSON `{ "dyn": [...], "mag": ..., "trig": [...], "dir": [...] }` |
| `/api/state` | `GET` | Active configuration, mode, local IP, and OSC target |
| `/api/cfg?gn=..&tx=..&sv=1` | `GET` | Updates `gn`, `tx`, `ty`, `tz`, `tm`, `s`, `ac`; saves to Flash if `sv=1` |
| `/api/cal` | `GET` | Zero-calibrates `A0..A2` |
| `/api/sleep` | `GET` | Enters Deep Sleep |
| `/api/wifi` | `GET` | Lists stored Wi-Fi SSIDs and IP mappings (passwords omitted) |
| `/api/wifi/add?s=..&p=..&d=..&l=..&g=..` | `GET` | Adds or updates a Wi-Fi entry in Flash |
| `/api/wifi/del?i=0` | `GET` | Deletes a Wi-Fi entry by index |
| `/api/scan` | `GET` | Scans nearby 2.4 GHz Wi-Fi networks |
| `/api/client` | `GET` | Switches from AP Mode to Client Mode |

---

## 5. Native Max/MSP Patch ([ArduinoMKR-1010-OSC-Streamer.maxpat](./ArduinoMKR-1010-OSC-Streamer.maxpat))

Built with **100% native Max objects** (tested on Max 9, no Spat5 or CNMAT externals required) and opens directly in **Presentation Mode** (`Cmd+E` toggles Patching Mode):

- **OSC Input (`[udpreceive 3033]`):** Max's `udpreceive` decodes OSC natively, routed through chained `[route]` objects.
- **Overlaid Oscilloscope:** 4 transparent-layered `[multislider]` objects (`X` Red, `Y` Green, `Z` Blue, `Mag` White, range `-1.2 .. +1.2`).
- **Bang Triggers & Direction:** 4 `[button]` bangs for `TRIG X/Y/Z/MAG` (with `/accel/trig/mag` wired to `[button]` -> `[s -external-arpeggio-start-]`) + 3 numeric direction displays (`+1 / 0 / -1`).
- **Real-Time Control (`[udpsend <MKR_IP> 3034]`):** Sliders initialized via `loadmess set` (`5.0`, `0.60`, `0.60`, `0.18`), **Calibrate Zero X/Y/Z** and **Deep Sleep** buttons, and an `[attrui]` (`host`) field to edit the MKR's target IP without unlocking the patch.

---

## 6. OSC Protocol Reference

### Outgoing OSC (MKR -> UDP Port `3033`)

| Address | Type / Range | Description |
|---|---|---|
| `/accel/x`, `/y`, `/z` | `float` (`-1..1`) | Calibrated tilt zoomed by `accelGain` |
| `/accel/dyn/x`, `/y`, `/z` | `float` (`-1.5..1.5`) | High-pass dynamic motion (gravity-free) |
| `/accel/mag` | `float` (`>=0`) | 3D dynamic magnitude `sqrt(dx^2 + dy^2 + dz^2)` |
| `/accel/trig/x`, `/y`, `/z`, `/mag` | `int` (`1`) | Pure bang emitted when `\|dyn\| >= threshold` (default `0.60`, re-arms at 70%) |
| `/accel/dir/x`, `/y`, `/z` | `int` (`+1 / 0 / -1`) | Slope direction: `+1` ascending, `-1` descending, `0` steady |
| `/accel/calibrated` | `int` (`1`) | Emitted when zero calibration completes |
| `/analog/A0..A6` | `int` (`0..4095`) | Raw 12-bit ADC value (sent when delta `>= 4`) |
| `/analog-one/A0..A6` | `float` (`0..1`) | Clipped, smoothed & gamma-shaped value |
| `/analog-hundred/A0..A6` | `float` (`-100..100`) | Scaled output |
| `/digital/D2..D14` | `int` (`0/1`) | Digital inputs (excluding `D6` LED and `D7` switch) |
| `/switch/raw` | `int` (`1/0`) | Immediate press/release state of switch `D7` |
| `/switch/short`, `/double`, `/triple`, `/long` | `int` (`1`) | Switch gesture events |
| `/status/sleep` | `int` (`1/0`) | `1` entering Deep Sleep, `0` woke up |
| `/timestamp` | `int` | Uptime in ms (`1 Hz` heartbeat) |

### Incoming OSC (Max -> MKR UDP Port `3034`)

| Address | Arguments | Effect |
|---|---|---|
| `/accel/gain` | `<float 1..30>` | Set accelerometer zoom gain |
| `/accel/thresh` | `<float>` or `<fx fy fz>` | Set dynamic threshold(s) (`0.01..2.0`) |
| `/accel/threshmag` | `<float 0.01..3.0>` | Set 3D magnitude threshold |
| `/accel/calibrate` | `<any>` | Run 16-sample zero calibration on `A0..A2` |
| `/config/smooth` | `<float 0.01..1.0>` | Set exponential smoothing factor |
| `/config/gamma` | `<float 0.1..5.0>` | Set gamma curve |
| `/config/clip` | `<min max>` | Set raw ADC clip range (`0..4095`) |
| `/config/rate` | `<int 10..1000>` | Set sampling rate in Hz |
| `/config/analogcount` | `<int 1..7>` | Set number of active analog pins (`A0..A(n-1)`) |
| `/config/pause` | `<1\|0>` | Pause/resume OSC streaming |
| `/system/save` | `<any>` | Save current parameters to Flash |
| `/system/sleep` | `<1>` | Enter Deep Sleep immediately |
