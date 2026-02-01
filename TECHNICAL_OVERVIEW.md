# Eaglehagen — Technical Overview

## Hardware Platform

**LilyGO T-Display S3** — ESP32-S3 with integrated 170×320 TFT display (portrait). Two physical buttons: IO14 (pump start) and GPIO0 (BOOT0, format toggle). USB-C with CDC serial for host communication.

---

## Pin Assignment

| Pin | Function | Notes |
|-----|----------|-------|
| GPIO 17 | UART1 TX → MaCO2 | 9600 baud, 8N1 |
| GPIO 18 | UART1 RX ← MaCO2 | 9600 baud, 8N1 |
| GPIO 1 | ADC — O2 sensor | 12-bit, 11dB atten (0–3.3V) |
| GPIO 2 | ADC — Volume sensor | 12-bit, weak pull-down |
| GPIO 14 | Button — Pump start | INPUT_PULLUP, interrupt-driven |
| GPIO 0 | Button — Format toggle | INPUT_PULLUP, interrupt-driven (BOOT0) |
| GPIO 15 | TFT power enable | OUTPUT, active HIGH |

---

## Sensor Interfaces

### MaCO2 CO2 Sensor (UART)

- **Protocol:** 8-byte packets at 8 Hz over UART1 (9600/8N1)
- **Init handshake:** Sensor sends `0x06`, firmware ACKs with `0x1B`, then 7 init bytes are discarded
- **Packet structure:**

| Byte | Field | Range | Description |
|------|-------|-------|-------------|
| d[0] | status1 | 6 | Valid-data flag (must be 0x06) |
| d[1] | status2 | 0–15 | Bit 0: pump, Bit 1: leak, Bit 2: occlusion |
| d[2] | rr | 0–60 | Respiratory rate (breaths/min) |
| d[3] | fico2 | 0–3 | FiCO2 inspired baseline (mmHg) |
| d[4] | fco2_wave | 0–50 | Real-time CO2 waveform (mmHg, 8 Hz) |
| d[5] | fetco2 | 0–120 | Sensor EtCO2 — **unreliable, not used** |
| d[6] | reserved | — | Ignored |
| d[7] | checksum | 0–255 | `sum(d[0..6]) & 0xFF` |

- **EtCO2 tracking:** Software peak detector watches d[4] waveform. Peak is latched when waveform drops below 25% of tracked peak (breath cycle boundary). Sensor's d[5] is unreliable and ignored.
- **Sync recovery:** After 3 consecutive checksum failures, enters sliding-window sync search using header + checksum + range validation. Flushes and restarts after 5 s timeout.

### O2 Sensor (ADC) — Servomex PM1111E

- Paramagnetic O2 sensor (no reagent consumption, no drift over time)
- Analog voltage output, 0–1V linear over measurement range
- ESP32 12-bit ADC with `esp_adc_cal` voltage correction
- Moving-average filter (configurable size, default 5)
- Calibration: two-point linear (`v_at_0%`, `v_at_100%`)

### Volume Sensor (ADC)

- Analog pressure/flow transducer
- Same ADC path as O2 (filtered, calibrated)
- Calibration: linear (`ml_per_volt`, `offset_ml`)

---

## Class Architecture

```
main.cpp  (orchestrator — setup, loop, timing)
  │
  ├── MaCO2Parser      UART1 packet parsing, EtCO2 tracking, commands
  ├── ADCManager       O2 + Volume ADC read, filter, calibration → CO2Data
  ├── DisplayManager   TFT LCD layout, waveform plot, numeric/status update
  ├── WiFiManager      AP, AsyncWebServer, WebSocket, JSON broadcast
  ├── DataLogger       Serial output formatting (legacy LabVIEW / ASCII)
  └── Button ×2        Interrupt-driven, debounced, short/long press
```

### Shared data structure: `CO2Data`

All classes read from or write into a single `CO2Data` instance in `main.cpp`:

| Field | Type | Filled by | Unit |
|-------|------|-----------|------|
| co2_waveform | uint16_t | MaCO2Parser | mmHg (raw) |
| fetco2 | uint8_t | MaCO2Parser (sw peak) | mmHg (raw) |
| fco2 | uint8_t | MaCO2Parser | mmHg (raw) |
| respiratory_rate | uint8_t | MaCO2Parser | bpm |
| status1, status2 | uint8_t | MaCO2Parser | flags |
| o2_adc | uint16_t | ADCManager | PIC-scaled (0–65535) |
| vol_adc | uint16_t | ADCManager | PIC-scaled (0–1023) |
| o2_percent | float | ADCManager | % |
| volume_ml | float | ADCManager | mL |
| valid | bool | MaCO2Parser | — |

> **Internal unit convention:** All CO2 values travel through the system as **raw mmHg**. Conversion to kPa (`× 0.133322`) happens at each output boundary (LCD, web display, serial, exports).

---

## Data Flow

```
MaCO2 sensor  ──UART──►  MaCO2Parser  ──► CO2Data.co2_waveform (mmHg)
                                        ──► CO2Data.fetco2      (mmHg, sw peak)
                                        ──► CO2Data.fco2        (mmHg)

O2 sensor     ──ADC───►  ADCManager   ──► CO2Data.o2_percent   (%)
Volume sensor ──ADC───►                ──► CO2Data.volume_ml   (mL)
                                        ──► CO2Data.o2_adc / vol_adc (PIC-scaled)

CO2Data ──► DisplayManager   → LCD waveform (raw mmHg for plot scale)
                              → LCD numeric  (× 0.133322 → kPa)

CO2Data ──► WiFiManager      → WebSocket JSON (raw mmHg)
                                  └─► Browser JS  (× 0.133322 → kPa for display/chart)
                                  └─► dataLog[]   (× 0.133322 → kPa, used for CSV/JSON export)

CO2Data ──► DataLogger       → Legacy LabVIEW: CO2 × 1.33322 (kPa×10), O2 × 10 (%×10)
                              → Tab-separated:  CO2 × 0.133322 (kPa), O2 as-is (%)
```

---

## Output Formats

### Legacy LabVIEW (PIC-compatible)

```
<ESC> AAA <TAB> BBBBB <TAB> CCCCC <TAB> s1 s2 RR fi et <CR><LF>
```

| Field | Format | Value convention |
|-------|--------|-----------------|
| AAA | `%03d` | CO2 waveform, kPa × 10 (e.g. 53 = 5.3 kPa) |
| BBBBB | `%05d` | O2, % × 10 (e.g. 201 = 20.1%) |
| CCCCC | `%05d` | Volume ADC, PIC 10-bit scaled (0–1023) |
| s1 | `%c` | status1 byte |
| s2 | `%c` | status2 byte (0 replaced → 128) |
| RR | `%c` | Respiratory rate byte (0 replaced → 255) |
| fi | `%c` | FiCO2, kPa × 10 (0 replaced → 255) |
| et | `%c` | FetCO2, kPa × 10 (0 replaced → 255) |

### Tab-Separated ASCII

```
CO2_kPa<TAB>O2%<TAB>RR<TAB>Volume_mL<TAB>Status1<TAB>Status2<CR><LF>
```

All values in SI / display units. Toggle between formats via BOOT0 button.

---

## Timing

| Task | Interval | Rate |
|------|----------|------|
| Data acquisition (MaCO2 parse + ADC) | 100 ms | 10 Hz |
| LCD refresh | 50 ms | 20 Hz |
| WebSocket broadcast | 125 ms | 8 Hz |
| Serial host output | 100 ms | 10 Hz |

---

## WiFi / Web Interface

- **Mode:** Access Point (`EAGLEHAGEN`, no password)
- **Server:** ESPAsyncWebServer on port 80
- **WebSocket:** `/ws` — pushes JSON at 8 Hz, receives `{"cmd":"start_pump"}` / `{"cmd":"zero_cal"}`
- **Endpoints:** `/` (HTML, gzip-compressed Chart.js embedded), `/api/data`, `/api/command`, `/api/setFormat`
- **Exports:** CSV and JSON download from browser, all CO2 in kPa with units metadata in JSON

---

## LCD Layout (170 × 320, portrait)

```
┌──────────────────────────┐  y=0
│  Ornhagen          ●WiFi │  Header (30 px)
├──────────────────────────┤  y=30
│                          │
│   ┌─ grid ─────────────┐ │
│   │  CO2 waveform plot  │ │  Waveform area (120 px)
│   └─────────────────────┘ │     • scrolling line graph
│  CO2 Waveform            │     • auto-scaling min/max
├──────────────────────────┤  y=150
│  ┌────────────────────┐  │
│  │  FCO2              │  │  Metric box — real-time CO2
│  │     4.3            │  │     value in kPa (updates 8 Hz)
│  │     kPa            │  │
│  └────────────────────┘  │
│  ┌────────────────────┐  │
│  │  O2                │  │  Metric box — O2 percentage
│  │    20.9            │  │     (moving-average filtered)
│  │     %              │  │
│  └────────────────────┘  │
├──────────────────────────┤  y=250
│ [PUMP] [LEAK] [OCCL]    │  Status badges (green/red)
│ ────────────────────     │  Separator
│ SSID: EAGLEHAGEN        │  Network info
│ IP: 192.168.x.x         │
│ Out: LabVIEW            │  Output format note
└──────────────────────────┘  y=320
```

- Metric boxes only redraw when the value changes (avoids flicker).
- Status badges only redraw when `status2` or the format label changes.
- Waveform redraws every 50 ms; the plot area is cleared and redrawn each cycle.

---

## Web Page Layout

```
┌─────────────────────────────────────────────────────────┐
│ 🖼 Örnhagens Monitor          by Teknosofen             │  Header
│                    [Connected] [Pump] [Leak] [Occl]     │  Status badges (right-aligned)
├─────────────────────────────────────────────────────────┤
│ ┌──────────┐ ┌──────────┐ ┌──────┐ ┌──────┐ ┌────────┐ │  Metric cards
│ │ EtCO₂   │ │ CO₂      │ │ RR   │ │ O₂   │ │ Volume │ │
│ │ [kPa]   │ │ [kPa]    │ │[bpm] │ │ [%]  │ │  [mL]  │ │
│ │  5.3    │ │  4.1     │ │  14  │ │ 20.9 │ │   320  │ │
│ └──────────┘ └──────────┘ └──────┘ └──────┘ └────────┘ │
├─────────────────────────────────────────────────────────┤
│ ┌─────────────────────────────────────────────────────┐ │
│ │ CO₂ Waveform (scrolling, kPa)                      │ │  Chart.js
│ └─────────────────────────────────────────────────────┘ │  time-series
│ ┌─────────────────────────────────────────────────────┐ │  charts
│ │ O₂ (%)                                             │ │  (gzip-
│ └─────────────────────────────────────────────────────┘ │   embedded,
│ ┌─────────────────────────────────────────────────────┐ │   ~69 KB)
│ │ Volume (mL)                                         │ │
│ └─────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────┤
│ [Start Pump] [Zero Cal] [Save CSV] [Save JSON] [Clear] │  Controls
└─────────────────────────────────────────────────────────┘
```

### Metric cards
Five cards displayed in a responsive row. Values update live via WebSocket. CO2 values are converted from the raw mmHg payload to kPa in JavaScript on receive.

### Charts
Three scrolling time-series charts rendered by Chart.js (bundled, gzip-compressed, served from flash — no internet required). The chart buffer keeps the last N seconds of data. The CO₂ waveform chart mirrors what the LCD waveform shows.

### Controls

| Button | Action |
|--------|--------|
| Start Pump | Sends `{"cmd":"start_pump"}` via WebSocket → forwarded as `0xA5` to MaCO2 |
| Zero Calibration | Sends `{"cmd":"zero_cal"}` via WebSocket → forwarded as `0x5A` to MaCO2 |
| Save Data (CSV) | Downloads `dataLog` as CSV; CO2 columns in kPa with unit headers |
| Save Data (JSON) | Downloads `dataLog` as JSON with a `metadata.units` block |
| Clear Data | Clears the in-memory `dataLog` array and resets chart buffers |

### Status badges
- **Connected / Reconnecting** — green/red, reflects WebSocket state. Auto-reconnect every 3 s on disconnect.
- **Pump / Leak / Occl** — green (OK) or red (alarm) with pulse animation. Updated from `status2` bits in each WebSocket frame.
