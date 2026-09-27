# Eaglehagen CO₂ Monitor — Design & Technical Reference

This is the single design reference for the Eaglehagen monitor: hardware, sensor protocol, firmware architecture, interfaces, build setup and history. It combines the earlier documents `TECHNICAL_OVERVIEW.md`, `SOFTWARE_ARCHITECTURE.md`, `ESP32_MedAir_Implementation.md`, `CO2_Analyzer_Serial_Interface.md`, `LIBRARY_DEPENDENCIES.md`, `LOGO_IMPLEMENTATION_FINAL.md`, `Setup.txt` and `README.md`. Those files remain available in git history (last present in commit `65a4f93`).

For operating instructions, see [../USER_GUIDE.md](../USER_GUIDE.md).

Where the older documents contradicted each other, **the current source code (`src/`, `include/`) was taken as the truth**. Superseded statements are listed in [§13](#13-superseded-design-decisions-and-corrections) so the history isn't lost.

---

## Contents

1. [Background and goals](#1-background-and-goals)
2. [Hardware](#2-hardware)
3. [MaCO2-V3 CO₂ sensor and protocol](#3-maco2-v3-co-sensor-and-protocol)
4. [Legacy PIC16F876A system](#4-legacy-pic16f876a-system)
5. [Firmware architecture](#5-firmware-architecture)
6. [USB host output (LabVIEW / ASCII)](#6-usb-host-output-labview--ascii), including the PC monitor page (§6.4)
7. [WiFi and web interface](#7-wifi-and-web-interface)
8. [LCD user interface](#8-lcd-user-interface)
9. [Configuration constants](#9-configuration-constants)
10. [Build environment and libraries](#10-build-environment-and-libraries)
11. [Testing and developer troubleshooting](#11-testing-and-developer-troubleshooting)
12. [Future enhancements](#12-future-enhancements)
13. [Superseded design decisions and corrections](#13-superseded-design-decisions-and-corrections)

---

## 1. Background and goals

The project replaces a roughly 20-year-old monitoring setup built for Dr. Eaglehagen (Örnhagen). In that setup a **PIC16F876A** combined data from a **MedAir MaCO2-V3** CO₂ analyser with two analog readings (O₂ and volume) and sent them to a PC running **LabVIEW**.

Goals of the ESP32 replacement:
- One device replaces the PIC board and the need for a PC during normal monitoring.
- Local display (TFT) and remote display (WiFi web page on phone or tablet).
- USB virtual COM port with a PIC-style data stream, so the existing LabVIEW program can keep working.
- Full bidirectional control of the analyser (start pump, zero calibration) from the web, USB or a hardware button.

### Old vs new signal flow

**Legacy (split serial):**
```
                    ┌─────────────────────┐
          ┌────────>│  MedAir MaCO2-V3    │
          │  TX     │   CO2 Analyzer      │
    Host/LabVIEW    └──────────┬──────────┘
          │                    │ TX (data out) 9600 8N1
          │         ┌──────────▼──────────┐
          │  RX     │  PIC16F876A         │◄─── AN0 (O2 sensor)
          └─────────┤  RX: from MaCO2     │◄─── AN1 (Volume sensor)
                    │  TX: to host        │
                    └─────────────────────┘
1. Host TX ──► MaCO2 RX       (commands; bypasses PIC completely)
2. MaCO2 TX ──► PIC RX        (8-byte packets)
3. PIC samples AN0/AN1, combines, PIC TX ──► Host RX
```
The PIC never saw the commands. It only received and enriched data.

**New (ESP32):**
```
MaCO2-V3 ◄──UART1 (bidirectional, 9600)──► ESP32-S3 (T-Display S3)
O2 sensor ──► GPIO1 ADC                      │  ├─ TFT 170×320
Vol sensor ─► GPIO2 ADC                      │  ├─ USB-C CDC ◄──► PC / LabVIEW (data + commands)
                                             │  └─ WiFi AP   ◄──► browsers (data + commands)
```
- The ESP32 owns the full UART link, so commands are visible and logged.
- The onboard 12-bit ADC replaces the PIC's AN0/AN1.
- USB-C provides power, programming, debug output and the LabVIEW data stream.

### Operating modes

| Mode | Topology | Use |
|---|---|---|
| USB only | MaCO2 ↔ ESP32 ↔ USB-C ↔ PC/LabVIEW | Drop-in replacement for PIC |
| WiFi only | MaCO2 ↔ ESP32 ↔ WiFi ↔ browser | Standalone bedside monitoring, no PC |
| Dual | Both at once | LabVIEW logging plus quick checks on a phone |
| USB + PC monitor page | MaCO2 ↔ ESP32 ↔ USB-C ↔ Chrome/Edge page (§6.4) | Live display and unlimited CSV recording on a PC, no LabVIEW |

### Migration plan (from original design)

1. **Parallel testing:** keep the PIC system, build the ESP32, verify LabVIEW data against the PIC output.
2. **Soft migration:** use the ESP32 normally and keep the PIC as backup.
3. **Full migration:** retire the PIC hardware.
4. **Web-first (optional):** move away from LabVIEW and export data from the web UI.

Hardware migration: remove the PIC board, add the T-Display S3, connect the O₂ and volume sensors to the ESP32 ADC (with dividers if they output 5 V), and replace the old RS-232 cable with one USB-C cable. The MaCO2 serial link (9600 baud) is unchanged.

```
Old:  MaCO2 TX → PIC RX;  PIC TX → RS232 → PC;  PC → MaCO2 RX;  O2 → AN0;  Vol → AN1
New:  MaCO2 TX → ESP32 UART1 RX (level shifted);  ESP32 UART1 TX → MaCO2 RX;
      O2 → GPIO1;  Vol → GPIO2;  ESP32 USB-C → PC (virtual COM)
```

---

## 2. Hardware

### 2.1 LilyGO T-Display S3

- ESP32-S3 dual-core, 240 MHz, WiFi/Bluetooth, 16 MB flash, OPI PSRAM
- 1.9" ST7789 TFT, 170×320, driven over an **8-bit parallel bus** (TFT_eSPI setup 206), used in portrait orientation
- Two push buttons: **GPIO0 (BOOT)** and **GPIO14**
- USB-C with native USB CDC (programming, debug, host data, 5 V power)
- Pin-layout picture: [Lilygo-T-display_pinlayout.webp](Lilygo-T-display_pinlayout.webp)

### 2.2 Pin assignment (current firmware)

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO 17 | UART1 TX → MaCO2 RX | 9600 8N1, direct 3.3 V drive |
| GPIO 18 | UART1 RX ← MaCO2 TX | 9600 8N1, **via level shifter** |
| GPIO 1 | ADC — O₂ sensor | 12-bit, 11 dB attenuation |
| GPIO 2 | ADC — Volume sensor | 12-bit, internal weak pull-down enabled |
| GPIO 14 | Button — pump start | INPUT_PULLUP, interrupt driven |
| GPIO 0 | Button — output-format toggle (BOOT) | INPUT_PULLUP, interrupt driven; strapping pin |
| GPIO 15 | TFT/peripheral power enable | OUTPUT, driven HIGH at start |
| GPIO 38 | TFT backlight | Handled by TFT_eSPI |
| GPIO 5–8 (+ data bus) | TFT parallel interface (RST=5, CS=6, DC=7, WR=8) | Pre-wired on the board |

### 2.3 Power supply

```
+12 V in ──┬──► MaCO2 sensor (+12 V)
           └──► 7805 ──► +5 V ──► ESP32 USB-C / 5 V
                [100 nF + 10 µF on input and output]
```
- **7805** (TO-220): input 7–35 V, output 5 V at up to 1 A. Add a heatsink if the load exceeds about 500 mA. If it runs too hot, use a buck converter (e.g. LM2596).
- **Power budget (estimated):** MaCO2 about 200–300 mA at 12 V. ESP32-S3 about 200–300 mA at 5 V (WiFi about 80 mA of that), backlight about 100 mA. Total at 5 V is about 300–400 mA.
- The board can also be powered from the PC over USB-C alone, for example on the bench.
- Option: 18650 Li-ion battery for portable use (not implemented).

### 2.4 Level shifting (MaCO2 5 V TTL ↔ ESP32 3.3 V)

**MaCO2 TX → ESP32 RX:** resistor and Zener clamp.
```
MaCO2 TX (5 V) ── 1 kΩ ──┬──► ESP32 GPIO18 (RX)
                         │
                   3.3 V Zener (1N5226B / BZX79-C3V3), cathode to signal
                         │
                        GND
```
This is simple and reliable at 9600 baud.

**ESP32 TX → MaCO2 RX:** direct connection. 3.3 V is above the 5 V TTL high threshold (about 2.0 V).

**Alternative:** a 74LVC245 powered from 3.3 V (5 V-tolerant inputs), with DIR = HIGH and OE = LOW.

### 2.5 Analog inputs

- ESP32-S3 SAR ADC, 12 bits (0–4095), 11 dB attenuation (usable range about 0–3.1 V). Accuracy is about ±2 % and up to about 83 kS/s.
- Voltage correction uses the ESP-IDF 5 **`adc_cali` curve-fitting** scheme (eFuse based), with a linear fallback. The legacy `esp_adc_cal` API **must not** be used: with Arduino core 3.x, linking it next to the driver behind `analogRead()` makes ESP-IDF abort at boot (`check_adc_oneshot_driver_conflict`).
- **O₂ sensor:** Servomex **PM1111E** paramagnetic cell (no reagent consumption, no drift). Linear 0–1 V output, connected directly.
- **Volume sensor:** analog pressure/flow transducer.
- If a sensor outputs 0–5 V, add a divider: R1 = 10 kΩ (series) and R2 = 20 kΩ (to GND) gives 3.33 V at 5 V. 10k/22k gives 3.44 V, which is marginal.
- Add 100 nF from each ADC pin to GND for noise filtering.

### 2.6 Bill of materials

| Component | Part | Qty | Notes |
|---|---|---|---|
| Microcontroller | LilyGO T-Display S3 | 1 | 1.9" display |
| Regulator | LM7805 TO-220 | 1 | +12 V → +5 V |
| Level-shift resistor | 1 kΩ ¼ W | 1 | UART RX |
| Zener | 1N5226B 3.3 V 500 mW | 1 | UART RX clamp |
| Divider R1 / R2 | 10 kΩ / 20 kΩ ¼ W | 2 + 2 | Only for 5 V sensors |
| Ceramic cap | 100 nF | 6 | Decoupling + ADC filter |
| Electrolytic cap | 10 µF 25 V | 2 | 7805 in/out |
| Power connector | 2-pin screw terminal | 1 | +12 V in |
| Heatsink | TO-220 | 1 | If needed |
| Protoboard/PCB | — | 1 | |

Estimated cost about USD 25–30, excluding sensors.

---

## 3. MaCO2-V3 CO₂ sensor and protocol

### 3.1 Device

- **Medair MaCO2-V3 OEM module**, made by MedAir AB (Delsbo, Sweden)
- Side-stream capnography: NDIR CO₂ measurement, internal sample pump (about 50 mL/min, typical for MedAir products), moisture trap
- MedAir was acquired by **Nonin Medical, Inc.** in 2006. The technology lives on in Nonin RespSense and LifeSense products and the CAP201 OEM module. Contact: oem@nonin.com (OEM sales), info@nonin.com (support), https://www.nonin.com
- No manufacturer datasheet for the MaCO2-V3 is available online. The protocol below was reverse-engineered from the PIC firmware and the LabVIEW program, then **corrected against captured sensor data** during ESP32 development.

### 3.2 Serial settings

9600 baud, 8 data bits, no parity, 1 stop bit, no flow control, asynchronous.

### 3.3 Initialization handshake

```
MaCO2 → host : 0x06            (start byte)
host  → MaCO2: 0x1B (ESC)      (acknowledge)
MaCO2 → host : 7 bytes         (init data, discarded)
```
The firmware waits up to 10 s (`MaCO2Parser::initialize`). On timeout it logs a warning and continues, and the parser picks up packets whenever they start arriving.

> In the PIC system the 0x1B went to the host port, not to the sensor. In the ESP32 firmware the ACK is written back to the MaCO2.

### 3.4 Data packet (8 bytes, about 8 Hz) — verified mapping

| Byte | Field | Range | Description |
|------|-------|-------|-------------|
| d[0] | status1 | 6 | Header / data-valid flag, always 0x06 for valid data |
| d[1] | status2 | 0–15 | Status bits (see 3.5) |
| d[2] | rr | 0–60 | Respiratory rate, breaths/min |
| d[3] | fico2 | 0–3 | FiCO₂, inspired baseline (mmHg) |
| d[4] | fco2_wave | 0–50 | Real-time CO₂ waveform (mmHg), updated at 8 Hz |
| d[5] | fetco2 | 0–120 | Sensor EtCO₂. **Unreliable, not used.** |
| d[6] | reserved | — | Ignored |
| d[7] | checksum | 0–255 | `sum(d[0..6]) & 0xFF` |

### 3.5 Status byte 2 (d[1])

| Bit | Meaning | Firmware interpretation |
|---|---|---|
| 0 | Pump | **0 = running (OK), 1 = stopped (alarm)** |
| 1 | Leak | 1 = leak detected |
| 2 | Occlusion | 1 = occlusion detected |
| 3–7 | Reserved | — |

Example: `0b00000110` means leak and occlusion. The LabVIEW diagram's own comment confirms this: "Status 2, bit 0 = pump stopped, bit 1 leak, bit 2 occlusion". An older reverse-engineered doc wrongly assumed bit 0 = 1 meant "running".

### 3.6 Commands (host → MaCO2)

| Byte | Command |
|---|---|
| `0xA5` | Start pump |
| `0x5A` | Zero calibration |
| (`F1` / `F2`) | Not command bytes. They are the LabVIEW **function keys**: F1 "Restart pump" sends `0xA5` and F2 "Zero CO2" sends `0x5A` (see §4.5). |

### 3.7 Firmware parsing details (`MaCO2Parser`)

- **Validation:** checksum must match, RR ≤ 60, and d[0] = 6 for `valid = true`. Sanity checks also reject EtCO₂ > 150.
- **Software EtCO₂:** the peak of d[4] is tracked during expiration. When the waveform falls below 25 % of the tracked peak (peak > 5, falling edge), the peak is latched as EtCO₂ and tracking restarts. The value stays 0 until the first detected breath.
- **Sync recovery:** after more than 3 consecutive errors, the parser collects bytes in a 16-byte sliding window. It looks for a 0x06 header with a valid checksum and plausible RR, waveform (≤ 50) and EtCO₂ (≤ 120). If no sync is found within 5 s, it flushes the UART buffer and restarts. Diagnostic lines prefixed with `#` go to the USB serial port.
- Statistics: `getPacketCount()`, `getErrorCount()`, `getLastPacketTime()`.

### 3.8 Clinical reference

- RR: breaths/min, adults typically 8–30.
- EtCO₂: normal 35–45 mmHg ≈ 4.5–6.0 kPa.
- FiCO₂: should be near 0. Raised values indicate rebreathing.
- Zero or absent CO₂ can mean no breathing, a disconnected cannula, occlusion or a malfunction.

---

## 4. Legacy PIC16F876A system

Kept here as a reference for LabVIEW compatibility and in case the old hardware needs service.

### 4.1 Hardware

- PIC16F876A at 20 MHz. UART TX = RC6, RX = RC7, 9600 8N1.
- **AN0 (RA0), O₂:** 10-bit ADC with Vref+ = AN3 and Vref− = Vss, left-justified, reported as 0–65535.
- **AN1 (RA1), Volume:** 10-bit ADC with Vdd/Vss reference, right-justified, 0–1023. LabVIEW labels it "Vol in ADC_Counts" / "DC_counts/liter".
- ADC acquisition about 20 µs plus conversion per channel. The PIC runs a continuous polling loop with no delays, so the frame rate follows the sensor.

### 4.2 PIC output frame

```
<ESC><CO2_ASCII><TAB><AN0_ADC><TAB><AN1_ADC><TAB><S1><S2><RR><B4><B5><CR><LF>
```
| # | Field | Encoding |
|---|---|---|
| 1 | ESC 0x1B | Start marker |
| 2 | CO₂ | 3 ASCII digits of d[4] (000–255) |
| 3,5,7 | TAB 0x09 | Separators |
| 4 | AN0 | 5 ASCII digits 00000–65535 |
| 6 | AN1 | 5 ASCII digits 00000–01023 |
| 8 | Status1 | Raw byte d[0] |
| 9 | Status2 | Raw byte d[1], 0 → 128 |
| 10 | RR | Raw byte d[2], 0 → 255 |
| 11 | d[4] | Raw byte, 0 → 255 (labelled "FetCO2" in the PIC analysis) |
| 12 | d[5] | Raw byte, 0 → 255 (labelled "FiCO2" in the PIC analysis) |
| 13–14 | CR LF | End of frame |

Example: `<ESC>045<TAB>32768<TAB>00512<TAB>[0x20][0x80][0x0F][0x2D][0x05]<CR><LF>`, meaning CO₂ 45, AN0 32768, AN1 512, S1 0x20, S2 0x80 (originally 0), RR 15, 45, 5.

How the LabVIEW program actually decodes this frame is described in §4.5.

### 4.3 Zero replacement and other quirks

- Zeros are replaced (Status2 → 128, RR/CO₂ bytes → 255) so no 0x00 byte reaches the host, where it could act as a string terminator. The PIC's `putchar()` refuses to send 0x00.
- As a result, 128 and 255 **can't be told apart from real zeros**.
- **Overrun recovery** runs once at start-up: read and discard two characters, clear CREN, set CREN.
- Limitations: no checksum on host frames, mixed ASCII and binary, fixed 9600 baud, no flow control.

### 4.4 Parsing pseudocode (PIC-style frame)

```python
def parse_frame(data):
    if data[0] != 0x1B:
        return None
    parts = data[1:].split(b'\t')
    co2, an0, an1 = int(parts[0]), int(parts[1]), int(parts[2])
    b = parts[3]
    status1 = b[0]
    status2 = 0 if b[1] == 128 else b[1]
    rr      = 0 if b[2] == 255 else b[2]
    x4      = 0 if b[3] == 255 else b[3]
    x5      = 0 if b[4] == 255 else b[4]
    return dict(co2=co2, an0=an0, an1=an1, status1=status1,
                status2=status2, rr=rr, byte4=x4, byte5=x5)
```
Synchronize on ESC (0x1B). Each frame ends with CR LF. A binary byte could itself be 0x09 or 0x0D, so a robust parser should use fixed positions after the third TAB.

### 4.5 The LabVIEW host program

Decoded from the block diagram of `Örnhagens O2-CO2-VOL-mätning_4.35_2_2026-0106.vi`.

**Frame, as stated in the diagram comment:** `<ESC>ABC<\t>DEFGH<\t>IJKLM<\t>OPQRS<\r><\n>`, **24 bytes**. The comment calls O "status 2" and P "status 1", but the wiring treats O as status 1.

**Reading:** `VISA Read` with a fixed byte count of **24** per loop iteration. The raw buffer is shown as hex and in `\`-codes format. The string is then cut at the TABs (Split String, String Subset offset 1 to skip each TAB).

| Field | LabVIEW processing | Expected content |
|---|---|---|
| ABC | Scan integer → "CO2 in mmHg" → ÷ **7.60** → "Convert to [%]" → × "CO2 Gain" → waveform chart | CO₂ in **mmHg** (7.6 mmHg ≈ 1 % at 760 mmHg) |
| DEFGH | Scan integer → "O2 in .1 %" → ÷ **10** → chart | O₂ in **0.1 %** units |
| IJKLM | Scan integer → "Vol in ADC_Counts" → ÷ "ADC_counts/liter" → chart | Volume, raw ADC counts |
| Binary tail | String → U8 array, then Index Array: | |
| index 1 | `= 6` → "Data Valid" (Status 1) | status1 |
| index 2 | Status 2 → bit 0 "Pump" (stopped), bit 1 "Leak", bit 2 "Occlusion" | status2 |
| index 3 | "RR" (U8) | RR |
| index 5 | "FiCO2" (U8) | see note |
| index 6 | "FetCO2" (U8) | see note |

**Note on indices 5 and 6** (inferred from the screenshot, not verified): the array most likely starts with the TAB before O, giving `[TAB, O, P, Q, R, S, CR, LF]`. Index 1 = O is then the 6-valued status1, which is consistent with the "Data Valid" check. Index 5 is then S and index 6 is CR (13). So the LabVIEW "FiCO2" indicator probably shows the last binary byte, and "FetCO2" shows a constant 13. Index 4 (R) is never read. Only the waveform, O₂, volume, RR and status are used for charts and alarms, so this doesn't affect the main display.

**Controls:**
- **F1 "Restart pump"** writes `A5` to the port. **F2 "Zero CO2"** writes `5A`. The diagram comment reads "Command A5:hex = start pump, 5A:hex Zero calibrate".
- **F5 / F6 / F7** adjust the sweep time (chart X-scale max). "Sample rate [1/s]" sets the X-scale multiplier.
- Data can be logged to file ("open or create" file refnum).

**Framing risks:**
- A fixed 24-byte read keeps frames aligned only if every frame is exactly 24 bytes and nothing else is sent on the port. One stray text line can shift all following reads. This is why the firmware mutes diagnostic text in Legacy mode (§5.8).
- If the VISA port is configured with a termination character (VISA default: enabled, `\n` = 0x0A), a read also stops early at any 0x0A byte. A binary byte equal to 10 (e.g. RR = 10, or status2 = 0x0A) then splits a frame. The PIC had the same weakness. The zero replacement only removes 0x00.

**Compatibility with the ESP32 firmware:**

| Field | LabVIEW expects | Firmware sends | Match |
|---|---|---|---|
| ABC | mmHg | mmHg | Yes |
| DEFGH | O₂ × 10 | O₂ × 10 | Yes |
| IJKLM | ADC counts 0–1023 | ADC counts 0–1023 | Yes |
| status1 / status2 / RR | raw | raw (with zero replacement) | Yes |
| bytes R, S | FiCO₂, FetCO₂ (see note) | FiCO₂ mmHg, EtCO₂ mmHg | Same units as the PIC |

The existing LabVIEW program works unchanged with the Legacy format.

**Frame sync:** the LabVIEW program has a separate preceding sequence, not visible in the diagram above, that searches for the **ESC byte (0x1B)** before the 24-byte reads. The firmware starts every frame with ESC, just as the PIC did (confirmed by the program's owner, 2026-09-27).

---

## 5. Firmware architecture

### 5.1 Modules

```
main.cpp  (orchestrator — setup, loop, timing, command routing)
  ├── MaCO2Parser     UART1 packet parsing, validation, sync recovery, EtCO2 tracking, commands
  ├── ADCManager      O2 + volume ADC read, moving-average filter, calibration → CO2Data
  ├── DisplayManager  TFT layout, waveform, metric boxes, status badges, splash screens
  ├── WiFiManager     Soft-AP, AsyncWebServer, WebSocket, JSON broadcast, embedded web page, command queue
  ├── DataLogger      USB CDC host output (Legacy LabVIEW / tab-separated ASCII)
  ├── DebugOut        Gated diagnostic text on the shared USB port (DebugLog.h)
  └── Button ×2       Interrupt-driven, debounced (50 ms), short/long press (1000 ms)
```
```
  UART1 (MaCO2)   GPIO ADC   TFT (parallel)   WiFi/Web   USB CDC
       ▲             ▲             ▲              ▲          ▲
  MaCO2Parser    ADCManager   DisplayManager  WiFiManager  DataLogger
       └─────────────┴──────── main.cpp ─────────┴──────────┘
```

**Files:** `src/*.cpp`, headers in `include/` (`MaCO2Parser.h`, `ADCManager.h`, `DisplayManager.h`, `WiFiManager.h`, `DataLogger.h`, `DebugLog.h`, `Button.hpp`, `ChartJS.h`, `TFT_eSPI_Setup.h`, and the generated `MonitorPage.h`). `tools/` holds the PC monitor page and `scripts/` its build-time embedding script (§6.4). The folders `EagleHagen/` and `_older_stuff/` contain earlier code that isn't built.

### 5.2 Key interfaces

```cpp
// MaCO2Parser
bool initialize(HardwareSerial& serial, unsigned long timeout_ms = 10000);
bool parsePacket(HardwareSerial& serial, CO2Data& data);   // non-blocking
void sendCommand(HardwareSerial& serial, MaCO2Command cmd); // CMD_START_PUMP=0xA5, CMD_ZERO_CAL=0x5A
bool isPumpRunning(const CO2Data&) const;   // (status2 & 0x01) == 0
bool isLeakDetected(const CO2Data&) const;
bool isOcclusionDetected(const CO2Data&) const;
bool isDataValid(const CO2Data&) const;

// ADCManager
bool begin();
void update(CO2Data& data);
void setO2Calibration(float v_at_0_percent, float v_at_100_percent);
void setVolumeCalibration(float ml_per_volt, float offset_ml);
void setFilterSize(uint8_t size);          // class default 10, main.cpp sets 5

// DisplayManager
bool begin();
void showSplash(const char* title, const char* subtitle = nullptr);
void clearScreen();
void updateAll(const CO2Data& data);
void addWaveformPoint(uint16_t co2_value);
void setNetworkInfo(const char* ssid, const char* ip);
void setOutputFormatName(const char* name);
void setBacklight(uint8_t brightness);

// WiFiManager
bool beginAP(const char* ssid, const char* password);      // empty password → open AP
bool beginStation(const char* ssid, const char* password, timeout);
bool startServer();
bool beginHostname(const char* hostname);  // mDNS + AP DNS answer for <hostname>.local
void update(const CO2Data& data);    // WebSocket broadcast
void loop();
bool hasCommand();  uint8_t getCommand();
void setDataLogger(DataLogger*);     // lets the web page change output format

// DataLogger
void sendData(Stream&, const CO2Data&);   // dispatches on current format
void setOutputFormat(OutputFormat);        // FORMAT_LEGACY_LABVIEW=0, FORMAT_TAB_SEPARATED=1; also gates DebugOut
void setOutputEnabled(bool);

// DebugOut (global DebugLogger, a Print subclass)
DebugOut.printf(...); DebugOut.println(...);  // use instead of Serial.print* for all diagnostics
void setEnabled(bool);                        // called by DataLogger
```

### 5.3 Shared data structure `CO2Data`

| Field | Type | Filled by | Unit |
|---|---|---|---|
| co2_waveform | uint16_t | MaCO2Parser (d[4]) | mmHg |
| fetco2 | uint8_t | MaCO2Parser (software peak) | mmHg |
| fco2 | uint8_t | MaCO2Parser (d[3], FiCO₂) | mmHg |
| respiratory_rate | uint8_t | MaCO2Parser | bpm |
| status1, status2 | uint8_t | MaCO2Parser | flags |
| o2_adc | uint16_t | ADCManager | PIC-scaled 0–65535 |
| vol_adc | uint16_t | ADCManager | PIC-scaled 0–1023 |
| o2_percent | float | ADCManager | % |
| volume_ml | float | ADCManager | mL |
| timestamp | uint32_t | millis() | ms |
| valid | bool | MaCO2Parser | — |

> **Unit convention:** CO₂ values travel through the firmware as **raw mmHg**. Conversion to kPa (× 0.133322) happens only at the output boundary: LCD, browser JavaScript, serial output and exports.

### 5.4 Data flow

```
MaCO2 ──UART──► MaCO2Parser ──► co2_waveform, fetco2 (sw peak), fco2, rr, status
O2    ──ADC───► ADCManager  ──► o2_percent, o2_adc
Vol   ──ADC───►             ──► volume_ml, vol_adc

CO2Data ──► DisplayManager → waveform (mmHg scale), numbers (kPa)
        ──► WiFiManager    → WebSocket JSON (mmHg) → browser converts to kPa (display, charts, CSV/JSON)
        ──► DataLogger     → Legacy: CO2 in mmHg, O2 as %×10  │ ASCII: CO2 kPa, O2 %
```

ADC readings are taken only when a new sensor packet arrives, so the ADC rate follows the sensor rate (about 8 Hz).

### 5.5 Timing (main loop, non-blocking)

| Task | Interval | Rate |
|---|---|---|
| Data acquisition (parse + ADC) | 100 ms | 10 Hz, polled faster than the sensor's 8 Hz to avoid buffer build-up |
| LCD refresh | 50 ms | 20 Hz |
| WebSocket broadcast | 125 ms | 8 Hz |
| USB host output | 100 ms | 10 Hz, only when `valid` |

`main.cpp` defines `LABVIEW_UPDATE_INTERVAL_MS = 200`, but the host-output block actually uses `DATA_UPDATE_INTERVAL_MS` (100 ms). The measured rate is about 9–10 frames/s, limited by the sensor's ~8–10 Hz packet rate. The web JSON export's `sample_rate_hz: 8` refers to the WebSocket rate.

### 5.6 Command routing

| Source | Path |
|---|---|
| IO14 button (short press) | `CMD_START_PUMP` → MaCO2 |
| BOOT button | Toggle `DataLogger` format; LCD `Out:` label updated |
| Web (WebSocket `{"cmd":"start_pump"|"zero_cal"}` or `POST /command`) | Queued in WiFiManager → MaCO2 |
| USB CDC byte `0xA5` / `0x5A` | Forwarded to MaCO2; other bytes ignored |
| Web `GET /api/setFormat?format=0|1` | Sets DataLogger format |

### 5.7 Start-up sequence

1. USB CDC at 115200. Banner is printed.
2. Display init. Splash "Teknosofen / Initializing…".
3. ADC init: volume-pin pull-down, filter 5, calibration.
4. Splash "Connecting sensor…". UART1 at 9600. MaCO2 handshake (≤ 10 s).
5. WiFi AP, web server, mDNS and DNS responder for `eaglehagen.local`.
6. DataLogger (default **Legacy LabVIEW**), linked to WiFiManager.
7. Buttons.
8. Splash "Ready! / IP: …". Main screen.

Because the default format is Legacy, the start-up log is muted (§5.8). Build with `-DDEBUG_LOG_ALWAYS=1` to see it.

### 5.8 Diagnostic output (`DebugLog.h`)

Diagnostics and host data share the single USB CDC port. All firmware diagnostics go through `DebugOut` (never `Serial.print*` directly):

| Host output mode | Diagnostics |
|---|---|
| Legacy LabVIEW (output enabled) | **Muted.** The port carries only 24-byte frames (§4.5). |
| Tab-separated ASCII | Sent, with every line prefixed `# ` (lines already starting with `#` are left alone) |
| Host output disabled | Sent, prefixed |

`DataLogger::setOutputFormat()` / `setOutputEnabled()` update the gate automatically. `-DDEBUG_LOG_ALWAYS=1` in `build_flags` forces diagnostics on for development. Note that this breaks LabVIEW framing.

Remaining caveat: web-server callbacks run in the AsyncTCP task. In ASCII mode, a diagnostic line from that task can in rare cases interleave with a data line.

---

## 6. USB host output (LabVIEW / ASCII)

USB-C enumerates as a CDC virtual COM port (`ARDUINO_USB_CDC_ON_BOOT=1`, ESP32-S3 USB-Serial/JTAG, VID:PID `303A:1001`): "USB Serial Device (COMx)" on Windows ("Seriell USB-enhet" in Swedish), `/dev/ttyACM0` on Linux. Chrome's port picker may list it as "USB JTAG/serial debug unit". The baud rate doesn't matter for USB CDC, and 115200 8N1 is the convention. The same port carries programming, debug messages and data.

> Diagnostic text is muted in Legacy mode and `#`-prefixed in ASCII mode (§5.8). Parsers of the ASCII format should skip lines starting with `#`.

### 6.1 Legacy LabVIEW format (default)

```
<ESC> AAA <TAB> BBBBB <TAB> CCCCC <TAB> s1 s2 RR fi et <CR><LF>
```
| Field | Format | Value |
|---|---|---|
| AAA | `%03d` | CO₂ waveform, **mmHg** (as the PIC; LabVIEW ÷ 7.60 → %) |
| BBBBB | `%05d` | O₂, **% × 10** (209 = 20.9 %) |
| CCCCC | `%05d` | Volume ADC, PIC 10-bit scaled 0–1023 |
| s1 | byte | status1 |
| s2 | byte | status2 (0 → 128) |
| RR | byte | respiratory rate (0 → 255) |
| fi | byte | FiCO₂, mmHg (0 → 255) |
| et | byte | EtCO₂ (software peak), mmHg (0 → 255) |

> **Compatibility:** all fields use the units the existing LabVIEW program expects (§4.5), so it works without changes. The frame is always exactly 24 bytes.

### 6.2 Tab-separated ASCII

```
CO2_kPa<TAB>O2%<TAB>RR<TAB>Volume_mL<TAB>Status1<TAB>Status2<CR><LF>
e.g.  4.3	20.9	14	320	6	0
```
CO₂ has 1 decimal (kPa), O₂ has 1 decimal (%), RR, volume (integer mL) and status bytes are decimal.

### 6.3 Selecting the format

BOOT button, web radio buttons, or `GET /api/setFormat?format=0` (legacy) / `1` (tab-separated). The setting isn't stored and resets to Legacy at power-up.

### 6.4 PC monitor page (`tools/Eaglehagen_Serial_Monitor.html`)

A stand-alone, single-file web page that displays and records the USB stream on a PC without LabVIEW. It has no dependencies: charts are drawn on `<canvas>`, the logo is embedded as base64, and nothing is loaded from the internet. User instructions are in section 7 of the user guide.

**It accepts both host output formats.** The parser handles the Legacy LabVIEW format (§6.1) and the tab-separated ASCII format (§6.2) in the same stream, so a BOOT-button switch while connected is followed without reconnecting. The *Data format* selector can force one format, but *Auto-detect* is the default.

**Distribution: the device carries its own copy.** The page is embedded in the firmware and offered as a download from the built-in web page (`GET /monitor.html`, §7.2), so no external files are needed. The intended workflow:
1. Over WiFi, open the device's web page and click **Download** under *PC monitor page (USB)*.
2. Open the saved file in Chrome or Edge, and connect with the USB cable.

The page can't run directly from `http://192.168.4.1`. Web Serial requires a secure context (`https://`, `localhost` or `file://`), and plain HTTP to the device is not one. That's why it's a download, not a route to browse. All three presentation paths remain: the LCD, the WiFi web page, and this page over USB.

**Build integration:**
- `scripts/embed_monitor_page.py`, a PlatformIO `pre:` extra script, gzips `tools/Eaglehagen_Serial_Monitor.html` at every build (level 9, `mtime=0`, so identical input gives identical output) into `include/MonitorPage.h` (`MONITOR_PAGE_GZ`, `MONITOR_PAGE_GZ_LEN`).
- The header is rewritten only when the page changes, so unchanged builds don't recompile. It's generated, so it's in `.gitignore`.
- The HTML file in `tools/` is the only source to edit. Every firmware build serves the matching version.
- Cost: about 56 KB of HTML (including the base64 logo) becomes about 28 KB of flash.

**Serial access:** Web Serial API, so only **Chrome and Edge** (desktop). It works from a `file://` URL. The browser's own port picker must be used once per device ("Add port…"). After that, `navigator.serial.getPorts()` lists the device in the page's own selector, and Espressif devices (USB VID `0x303A`) are pre-selected. Plug and unplug events are handled. Only one program can hold the COM port, so LabVIEW must be closed first.

**Parser** (`StreamParser`, between the `PARSER START` / `PARSER END` markers):
- **ESC (0x1B)** starts a candidate LabVIEW frame. It's accepted only if all 24 bytes validate: TABs at offsets 4, 10 and 16, CR LF at 22–23, and numeric ASCII fields. Otherwise the parser skips one byte and resyncs. Validating the whole frame keeps binary tail bytes equal to 0x1B or 0x0A (e.g. EtCO₂ = 27 mmHg, RR = 10) from breaking the framing.
- **Other bytes** are read as a text line up to LF. A line of six numeric TAB-separated fields is an ASCII frame. A line starting with `#` is a device message and goes to the log. Other printable text also goes to the log, and binary junk is counted as an error.
- Zero replacement is undone (Status2 128 → 0; RR, FiCO₂ and EtCO₂ 255 → 0). LabVIEW CO₂ values (mmHg) are converted to kPa.
- In ASCII format, EtCO₂ isn't transmitted. The page computes it with the firmware's peak-tracking algorithm (`EtTracker`, §3.7). FiCO₂ is unavailable.
- Volume is **ADC counts** in LabVIEW format and **mL** in ASCII format. The page labels the unit accordingly and stores them in separate CSV columns.
- **Tests:** `node tools/tests/test_monitor_parser.js` extracts the parser from the HTML and runs 10 checks. They cover a real device capture (`tools/tests/capture_labview.bin`) fed in odd-sized chunks, a mid-frame start, binary bytes that look like frame markers, ASCII with `#` lines, a format switch mid-stream, random garbage, and the EtCO₂ tracker. Run them after editing the parser.

**Recording:** every frame received while connected is kept, with no time limit (unlike the 2-minute web buffer). **CSV** columns: `Timestamp, Elapsed(s), CO2_Waveform(kPa), FetCO2(kPa), FiCO2(kPa), RR(bpm), O2(%), Volume(mL), Volume(ADC), Pump_Running, Leak_Detected, Occlusion_Detected, Status1, Status2, Format`. The *Swedish Excel* style uses `;`, decimal commas and a UTF-8 BOM. Files are saved through `showSaveFilePicker` where available, otherwise downloaded.

**Other features:**
- Start pump (`0xA5`) and zero calibration (`0x5A`, with confirmation) are written to the port.
- Dark and light themes. The theme, baud rate and CSV style are remembered in `localStorage`.
- Stale-data indication after 2 s without frames.
- `#demo` in the URL generates synthetic LabVIEW frames through the same parser.

---

## 7. WiFi and web interface

### 7.1 Network

- **Soft-AP** `EAGLEHAGEN`, **open** (no password), IP `192.168.4.1`.
- **Hostname `eaglehagen.local`** (`HOSTNAME` in `main.cpp`):
  - **mDNS** (`ESPmDNS`, `_http._tcp` service on port 80) for Windows, macOS, iOS and Linux.
  - A **DNS responder** (`DNSServer`, port 53) answers only `eaglehagen.local` with the AP IP. The AP's DHCP hands out the ESP32 as DNS server, and many Android devices resolve `.local` through unicast DNS instead of mDNS. All other names get NXDOMAIN, so phones don't show a captive-portal "sign in" pop-up.
  - `192.168.4.1` always works as a fallback.
- Station mode exists in `WiFiManager::beginStation` but isn't enabled (`WIFI_AP_MODE = true`).
- ESPAsyncWebServer on port 80. Multiple simultaneous clients are supported. About 8 KB/s per client at 8 Hz.

### 7.2 Endpoints

| Method / path | Purpose |
|---|---|
| `GET /` | Dashboard HTML, embedded in `WiFiManager.cpp` (`getIndexHTML()`) |
| `GET /chart.min.js` | Chart.js, **gzip-compressed and embedded** (`include/ChartJS.h`), so no internet is needed |
| `GET /monitor.html` | The PC monitor page (§6.4), gzip-embedded (`include/MonitorPage.h`, generated at build). Sent with `Content-Disposition: attachment` so the browser saves it as `Eaglehagen_Serial_Monitor.html`. |
| `WS /ws` | Pushes JSON at 8 Hz; accepts `{"cmd":"start_pump"}` / `{"cmd":"zero_cal"}` |
| `GET /data` | Current values as JSON |
| `POST /command` | Form field `cmd=start_pump|zero_cal` |
| `GET /api/setFormat?format=0|1` | Select host-output format |

### 7.3 WebSocket JSON (per frame)

```json
{ "timestamp": 123456, "co2_waveform": 32, "fetco2": 40, "fco2": 0, "rr": 14,
  "o2_percent": 20.9, "volume_ml": 320.0, "status1": 6, "status2": 0, "valid": true,
  "pump_running": true, "leak_detected": false, "occlusion_detected": false,
  "output_format": 0 }
```
`output_format` is the current host format (0 = Legacy, 1 = tab-separated). The page uses it to keep the format radio buttons in sync, including changes made with the BOOT button.
CO₂ fields are in **mmHg**. The browser converts them to kPa.

### 7.4 Page layout

```
┌─────────────────────────────────────────────────────────┐
│ [logo] Örnhagens Monitor                by Teknosofen   │
│                    [Connected] [Pump] [Leak] [Occl]     │
├─────────────────────────────────────────────────────────┤
│ End-Tidal CO₂ [kPa] │ CO₂ [kPa] │ RR [bpm] │ O₂ [%] │ Volume [mL] │
├─────────────────────────────────────────────────────────┤
│ CO₂ waveform chart (kPa)                                │
│ O₂ chart (%)                                            │
│ Volume chart (mL)                                       │
├─────────────────────────────────────────────────────────┤
│ [Start Pump] [Zero Calibration] [Save CSV] [Save JSON] [Clear Data] │
│ USB Host Output Format (10 Hz): (•) Legacy (LabVIEW) ( ) Tab-Separated ASCII │
│ Data Points: n / 960 (2 min buffer) | Duration m:ss                 │
│ PC monitor page (USB) – save it, open in Chrome/Edge     [Download] │
└─────────────────────────────────────────────────────────┘
```
- Status badges: Connected/Reconnecting reflects the WebSocket state, with auto-reconnect every 3 s. Pump/Leak/Occl turn red with a pulse animation on alarm (`Pump!`, `Leak!`, `Occl!`).
- Charts: Chart.js, updated with `update('none')` (no animation) for performance.
- Responsive layout for phone, tablet and desktop.
- The format radio buttons have no default in the HTML. They're set from `output_format` in each WebSocket frame.

### 7.5 Data buffer and export (client-side)

- `dataLog[]` in the browser keeps the last **960 samples** (2 min at 8 Hz). Older samples are shifted out.
- **CSV:** `medair_co2_data_<ISO-timestamp>.csv`, columns `Timestamp, Elapsed(s), CO2_Waveform(kPa), FetCO2(kPa), FiCO2(kPa), RR(bpm), O2(%), Volume(mL), Pump_Running, Leak_Detected, Occlusion_Detected, Status1, Status2`.
- **JSON:** `{ metadata: { export_time, recording_start, duration_seconds, data_points, sample_rate_hz: 8, device, firmware, units: {…} }, data: [...] }`.
- **Clear Data** empties `dataLog` and the chart buffers.

### 7.6 Logo and branding

- The web logo is the Örnhagen "E" WebP (234×250 px, about 10 KB, about 13.4 KB as base64). It's embedded in an `<img>` data-URI, 40 px high, left of the title.
- Why embed everything (HTML, CSS, JS, logo, Chart.js) in flash: no SPIFFS/LittleFS, a single firmware upload, simpler deployment, and the page works without internet.
- Colour palette shared with the TFT: soft grey background (#85BA / `TFT_LOGOBACKGROUND`), soft blue primary (#5497 / `TFT_LOGOBLUE`), steel-blue and soft-green accents.

---

## 8. LCD user interface

### 8.1 Layout (170 × 320 portrait)

```
┌──────────────────────────┐  y=0
│  Ornhagen          ●WiFi │  Header (30 px), FreeSansBold, green dot when WiFi up
├──────────────────────────┤  y=30
│  CO2 waveform (grid)     │  120 px scrolling line, auto min/max scaling
│  CO2 Waveform            │
├──────────────────────────┤  y=150
│  FCO2   4.3   kPa        │  Metric box (50 px), real-time CO2
│  O2    20.9   %          │  Metric box (50 px), filtered O2
├──────────────────────────┤  y=250
│ [PUMP] [LEAK] [OCCL]     │  Status badges (green OK / red alarm)
│ ───────────────          │
│ SSID: EAGLEHAGEN         │
│ IP: 192.168.4.1          │
│ Out: LabVIEW             │  Host output format
└──────────────────────────┘  y=320
```
- Metric boxes redraw only when the formatted value changes (anti-flicker).
- Badges redraw only when `status2` or the format label changes.
- The waveform area is cleared and redrawn every 50 ms.
- EtCO₂, RR and volume aren't shown on the LCD (space). They're on the web page.

### 8.2 Splash screens and logo decision

`showSplash(title, subtitle)` draws the title in FreeSansBold12pt7b (`TFT_LOGOBLUE` on `TFT_LOGOBACKGROUND`, centred) and the subtitle in the small font (`TFT_SLATEBLUE`). The title is currently **"Teknosofen"** during start-up, then "Ready!".

A bitmap logo on the TFT was tried and dropped. Monochrome bitmap conversion kept producing rendering artefacts, and text is cleaner on a small screen, simpler, and saves about 480 bytes of PROGMEM. If a bitmap is wanted later, the options are TFT_eSPI `pushImage()` with pre-converted arrays, another converter (LCD Image Converter, online TFT tools), or loading from a filesystem.

---

## 9. Configuration constants

All in `src/main.cpp`:

```cpp
const char* WIFI_SSID     = "EAGLEHAGEN";
const char* WIFI_PASSWORD = "";          // open AP (was "co2monitor")
const bool  WIFI_AP_MODE  = true;
const char* HOSTNAME      = "eaglehagen";   // http://eaglehagen.local

#define UART_RX_MACO2  18
#define UART_TX_MACO2  17
#define O2_SENSOR_PIN   1
#define VOL_SENSOR_PIN  2
#define BUTTON_PIN     14   // pump start
#define BOOT0_PIN       0   // format toggle

#define DATA_UPDATE_INTERVAL_MS     100
#define DISPLAY_UPDATE_INTERVAL_MS   50
#define WIFI_UPDATE_INTERVAL_MS     125
#define LABVIEW_UPDATE_INTERVAL_MS  200   // defined but unused, see §5.5

adcManager.setFilterSize(5);
adcManager.setO2Calibration(0.0, 1.0);        // V at 0 % and at 100 % O2 (PM1111E 0–1 V)
adcManager.setVolumeCalibration(200.0, 0.0);  // mL per volt, offset mL — placeholder, calibrate!
```
Buttons: `Button(pin, longPressMs = 1000, debounceMs = 50)`.

Calibration is currently compile-time only. A web calibration page or Preferences/NVS storage (WiFi credentials, alarm limits, brightness, sweep speed, units) is a future item.

---

## 10. Build environment and libraries

### 10.1 PlatformIO (used by this project)

```ini
[env:lilygo-t-display-s3]
; pioarduino 53.03.10 = Arduino core 3.1.0 / ESP-IDF 5.3 (pinned, tested on hardware)
platform = https://github.com/pioarduino/platform-espressif32/releases/download/53.03.10/platform-espressif32.zip
board = lilygo-t-display-s3
framework = arduino
extra_scripts = pre:scripts/embed_monitor_page.py   ; embeds the PC monitor page (§6.4)
build_flags =
    -DUSER_SETUP_LOADED=1
    -include $PROJECT_INCLUDE_DIR/TFT_eSPI_Setup.h
    -DLILYGO_T_DISPLAY_S3=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DCORE_DEBUG_LEVEL=0          ; no Arduino-core log lines on the LabVIEW port
    ; optional for development: -DDEBUG_LOG_ALWAYS=1
lib_deps =
    bodmer/TFT_eSPI@2.5.43
    bblanchon/ArduinoJson@7.4.2
    https://github.com/me-no-dev/ESPAsyncWebServer.git#ad3741d159f9cfd50c567e81b67f3bef1dc6d89f   ; 3.6.0
    https://github.com/me-no-dev/AsyncTCP.git#ef448a8a1dffe4ec1b72326dd5a26211ff227b49           ; 3.3.2
monitor_speed = 115200
monitor_filters = esp32_exception_decoder
```
**Everything is pinned** to the versions verified on the hardware (2026-09-27). Before this, the unpinned `platform = espressif32` silently moved to Arduino core 3.x, and the legacy ADC code then crash-looped at boot (§2.5). Upgrade deliberately: change one pin, `pio run -t clean`, flash, and check the USB frames and the display.

Commands: `pio run` (build), `pio run -t upload`, `pio device monitor`. No filesystem image is needed. Don't run `uploadfs`, because the web assets are compiled in.

### 10.2 TFT_eSPI configuration

The display setup is part of the project: [`include/TFT_eSPI_Setup.h`](../include/TFT_eSPI_Setup.h), a copy of TFT_eSPI's `Setup206_LilyGo_T_Display_S3.h` (ST7789, 170×320, 8-bit parallel, backlight GPIO38). `-DUSER_SETUP_LOADED=1` makes TFT_eSPI skip its own `User_Setup_Select.h`, and `-include` injects the project file into every compilation unit, including the library. The library files under `.pio/` are **not** edited, and a clean checkout builds as is.

PlatformIO does not track force-included headers. After changing `TFT_eSPI_Setup.h`, run `pio run -t clean` before building.

(Previously the library's `User_Setup_Select.h` line 133 had to be edited by hand to select Setup206. That edit was lost whenever `.pio/` was deleted.)

### 10.3 Libraries

| Library | Source | Purpose / used by | Licence |
|---|---|---|---|
| TFT_eSPI (Bodmer) 2.5.43 | PlatformIO registry | Display, DisplayManager | FreeBSD |
| ArduinoJson (B. Blanchon) 7.4.2 | PlatformIO registry | JSON for WebSocket/API, WiFiManager | MIT |
| ESPAsyncWebServer 3.6.0 (commit `ad3741d`) | GitHub URL (me-no-dev, now maintained as ESP32Async) | Async HTTP + WebSocket, WiFiManager | LGPL-3.0 |
| AsyncTCP 3.3.2 (commit `ef448a8`) | GitHub URL | Dependency of ESPAsyncWebServer | LGPL-3.0 |
| WiFi.h, ESPmDNS.h, DNSServer.h, esp_adc/adc_cali.h, HardwareSerial, Arduino.h | ESP32 Arduino core 3.1.0 | Built in | Apache-2.0 |

Measured footprint (2026-09-27 build): **1.23 MB flash** (18.7 % of the 6.5 MB application partition) and **49 KB static RAM** (15 %). Embedded assets: Chart.js about 69 KB gzip, the PC monitor page about 28 KB gzip. The board has 16 MB flash and 512 KB SRAM, so there's plenty of room.

Library links: TFT_eSPI https://github.com/Bodmer/TFT_eSPI · ESPAsyncWebServer https://github.com/me-no-dev/ESPAsyncWebServer · AsyncTCP https://github.com/me-no-dev/AsyncTCP · ArduinoJson https://arduinojson.org · ESP32 core https://github.com/espressif/arduino-esp32 · T-Display S3 https://github.com/Xinyuan-LilyGO/T-Display-S3 · Forum https://www.esp32.com

### 10.4 Arduino IDE (alternative)

PlatformIO is the supported build. The Arduino IDE can work, with these differences:

1. Boards Manager URL `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`, then install "esp32 by Espressif Systems" **3.1.0** (core 3.x is required: the ADC code uses `adc_cali`, and the DNS responder uses the async `DNSServer`).
2. Board settings: "LilyGO T-Display S3" or "ESP32S3 Dev Module". USB CDC On Boot: Enabled. CPU 240 MHz. Flash QIO 80 MHz, 16 MB. PSRAM: OPI. USB Mode: Hardware CDC and JTAG. Upload 921600.
3. Install TFT_eSPI and ArduinoJson from the Library Manager. Add ESPAsyncWebServer and AsyncTCP as .ZIP libraries from GitHub.
4. The IDE can't pass `-include`, so select the display setup in the library itself: in `TFT_eSPI/User_Setup_Select.h`, enable `#include <User_Setups/Setup206_LilyGo_T_Display_S3.h>` and comment out the default.
5. The IDE doesn't run `scripts/embed_monitor_page.py`, so `include/MonitorPage.h` must exist before compiling. Build once with PlatformIO to generate it, or remove the `/monitor.html` route.

### 10.5 Fallbacks if a library causes trouble

- **Minimal build:** remove WiFiManager. Display, sensor, ADC and USB output still work.
- Replace ESPAsyncWebServer with the built-in blocking `WebServer` (`WebServer server(80);`).
- Replace TFT_eSPI with LovyanGFX (faster) or Adafruit_ST7789 (simpler).

---

## 11. Testing and developer troubleshooting

### 11.1 Tests

- **Serial monitor** (`pio device monitor`, 115200): switch to ASCII format (BOOT button) to see `#`-prefixed diagnostics. To also see the start-up log, build with `-DDEBUG_LOG_ALWAYS=1`. `printStatus()` in `main.cpp` prints packet and error counts, WiFi clients, raw ADC and voltage, and flags. It isn't currently called, so hook it up temporarily when debugging.
- **Commands:** send the raw bytes 0xA5 or 0x5A over the USB port.
- **WebSocket** from a browser console:
  ```js
  const ws = new WebSocket('ws://192.168.4.1/ws');
  ws.onmessage = e => console.log(JSON.parse(e.data));
  ws.send(JSON.stringify({cmd: 'start_pump'}));
  ```
- **Display test:** inject a fake `CO2Data` (e.g. waveform 38, RR 16, status2 0) into `displayManager.updateAll()`.
- **PC monitor page parser:** `node tools/tests/test_monitor_parser.js` (§6.4).
- **Firmware:** `test/` is currently empty. Planned unit tests cover the MaCO2 parser (checksum, sync recovery, EtCO₂ peak detection) and the output formatters.
- **End-to-end USB check** used during development: read COM3 for 20 s and confirm that every frame is 24 bytes starting with ESC, with no bytes outside frames.

### 11.2 Troubleshooting

| Symptom | Cause / check |
|---|---|
| Display white/black | Build flags from §10.1 missing; stale build after editing `TFT_eSPI_Setup.h` (`pio run -t clean`); GPIO15 not driven high |
| No diagnostics on the serial monitor | Expected in Legacy format. Press BOOT (ASCII) or build with `-DDEBUG_LOG_ALWAYS=1`. |
| `'AsyncWebServer' does not name a type` | AsyncTCP missing; ESP32 core too old; wrong board |
| ArduinoJson errors | API differences between v6 and v7. The project uses **v7** (`doc["x"].is<T>()`, `JsonDocument`). Include `<ArduinoJson.h>` with that exact case. |
| Boot loop, `abort()` in `check_adc_oneshot_driver_conflict` | Legacy ADC API (`esp_adc_cal.h` / `driver/adc.h`) linked somewhere. Use only `analogRead()` and `esp_adc/adc_cali*.h`. |
| MaCO2 not initializing | TX/RX swapped? Level shifter? +12 V to the sensor? Firmware continues and resyncs anyway. |
| Frequent `# Checksum error` / sync lost | Noise or level-shifter problem, baud mismatch, ground loop |
| ADC values wrong | Dividers (5 V sensors), calibration constants, raw values via `printStatus()` |
| WiFi AP missing | Board stuck in boot mode (GPIO0 held at reset); check serial log |
| LabVIEW gets no data | Charge-only cable; wrong COM port; port held by another program (PIO monitor, the PC monitor page); `setOutputEnabled(true)`; no valid sensor data (output is sent only when `valid`) |
| `pio run -t upload` fails: "could not open port … Access denied" | Another program holds COM3 (PC monitor page, LabVIEW, serial monitor). Disconnect it, then upload. |
| Web page has no Download button after flashing | Browser cache: reload with Ctrl+F5 |
| Moisture / invalid readings | Water trap saturated; cannula or sample line |

---

## 12. Future enhancements

Merged from all earlier roadmaps:
- Persist settings (format, calibration, WiFi, alarm limits, brightness, sweep speed, mmHg/kPa) in NVS/Preferences.
- Web-based calibration page (O₂ two-point, volume scale/offset).
- WiFi station mode or configuration portal.
- On-device alarms with thresholds (EtCO₂/RR high and low, no CO₂ detected, pump stopped), shown on the LCD and web page.
- On-device logging (SD card or flash) for recordings without a connected PC. Long recordings over USB are already possible with the PC monitor page (§6.4). `DataLogger::enableCSVLogging` is a stub.
- MQTT or export to hospital systems. Cloud logging and trend analysis.
- BLE or native mobile app. Multi-sensor or multi-patient support.
- Battery (18650) power with monitoring.
- Unit tests in `test/`.

---

## 13. Superseded design decisions and corrections

Statements from the earlier documents that no longer match the firmware. They're kept here so nobody reintroduces them by mistake.

| Earlier statement | Current reality |
|---|---|
| MaCO2 UART on GPIO 43 (TX) / 44 (RX) | **GPIO 17 (TX) / 18 (RX)** |
| TFT on SPI (MOSI 19, SCLK 18, CS 5, DC 16, RST 23) | T-Display S3 uses an **8-bit parallel bus** (Setup206). Those SPI pins are from the original T-Display; GPIO18 is now UART RX. |
| RGB LED on GPIO 48 | Not present or used on this board |
| SSID `MedAir_Monitor`, password `co2monitor` | **`EAGLEHAGEN`, open (no password)** |
| O₂ calibration `(0.0, 3.3)` | **`(0.0, 1.0)`**, matching the PM1111E 0–1 V output |
| ADC filter default 10 | Class default 10, **set to 5** in `main.cpp` |
| All tasks at 8 Hz | Acquisition 10 Hz, LCD 20 Hz, web 8 Hz, USB output 10 Hz |
| Packet: d[3] unused, d[4] = FetCO₂, d[5] = FiCO₂, d[6–7] unused, no checksum | d[3] = FiCO₂, d[4] = waveform, d[5] = unreliable EtCO₂ (ignored), **d[7] = checksum** |
| Status2 bit 0: 1 = pump running | **0 = running, 1 = stopped** |
| EtCO₂ read from the sensor | **Computed in firmware** from waveform peaks |
| USB output identical to PIC; "no LabVIEW changes needed" | True again. For a while the firmware sent CO₂ as kPa×10, which LabVIEW (expecting mmHg) read about 33 % high. It now sends mmHg (§4.5). |
| `0xF1` is a "restart pump" command byte | F1 is the LabVIEW function key that sends `0xA5` |
| Debug text interleaved with LabVIEW data | Muted in Legacy mode, `#`-prefixed in ASCII mode (§5.8) |
| Web format selector defaults to Tab-Separated | Synced from the device via `output_format` |
| Web page only at `192.168.4.1` | Also at `eaglehagen.local` (mDNS + DNS) |
| Only the PIC/LabVIEW output format | Two formats (Legacy / tab-separated), switchable by button or web |
| Chart.js from CDN | **Embedded gzip** in flash, served at `/chart.min.js` |
| Web files in `data/` on SPIFFS; `pio run -t uploadfs` | Everything is embedded in firmware. No filesystem. |
| Endpoints `/api/data`, `/api/command`, `/command?cmd=` (GET), `/export?format=csv` | `/data`, `POST /command`, WebSocket commands, client-side export |
| LCD shows EtCO₂, FiCO₂/FCO₂, RR, O₂, Volume | LCD shows **FCO2 (kPa) and O₂** only, plus the `Out:` format line |
| Splash title "Ornhagen" / "MedAir CO2 Monitor" | Splash "Teknosofen"; header "Ornhagen"; web "Örnhagens Monitor" |
| ArduinoJson v6 recommended | **v7** in use |
| TFT_eSPI configured by editing the library's `User_Setup_Select.h` | Project file `include/TFT_eSPI_Setup.h`, force-included via build flags (§10.2) |
| ADC voltage via legacy `esp_adc_cal` | `adc_cali` curve fitting. The legacy API crash-looped the board at boot on Arduino core 3.x (§2.5). |
| For LabVIEW, connect USB-Serial to UART2 | LabVIEW uses the **USB-C CDC port** |
| Long recordings need a separate logging program | The PC monitor page records over USB without a time limit (§6.4) |
| PC monitor page only available as a separate file | Embedded in the firmware and downloadable from the web page (`/monitor.html`) |
| Class names `MedAirParser`, `WaveformBuffer`, `CO2WebServer`, `Config.h` (design phase) | Implemented as `MaCO2Parser`, `DisplayManager` (internal buffer), `WiFiManager`, constants in `main.cpp` |
