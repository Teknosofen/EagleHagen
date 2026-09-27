# Eaglehagen CO₂ Monitor — User Guide

This guide explains how to use the monitor day to day. For wiring, firmware and protocol details, see [documentation/DESIGN_REFERENCE.md](documentation/DESIGN_REFERENCE.md).

---

## 1. What the device does

The Eaglehagen monitor connects to a **MedAir MaCO2-V3** side-stream CO₂ analyser, an **O₂ sensor** and a **volume sensor**. It shows the readings in three places:

| Where | What you get |
|---|---|
| **Built-in screen** | CO₂ waveform, current CO₂ (kPa), O₂ (%), alarm badges, network info |
| **Phone / tablet / laptop over WiFi** | Live values, three scrolling charts, pump/calibration buttons, data export (CSV/JSON) |
| **PC over USB** | Continuous serial data stream for LabVIEW or any terminal/logging program |

All three can be used at the same time. No internet connection is needed.

---

## 2. Quick start

1. **Connect the sensors** (sample line to the MaCO2, O₂ and volume sensors) and apply **+12 V** power.
2. Wait about 5–15 seconds while the screen shows:
   `Teknosofen – Initializing…` → `Teknosofen – Connecting sensor…` → `Ready! IP: 192.168.4.1` → main screen.
3. On your phone or tablet, join the WiFi network **`EAGLEHAGEN`**. There is **no password**.
4. Open a browser and go to **`http://eaglehagen.local`** (or **`http://192.168.4.1`**, which always works)
5. If the **PUMP** badge is red, press the **pump button (IO14)** on the device or tap **Start Pump** on the web page.

That's it. The readings update live.

> If the sensor isn't connected at power-up, the device waits up to 10 seconds and then continues anyway. Data starts appearing as soon as the sensor begins sending.

---

## 3. The screen

```
┌──────────────────────────┐
│  Ornhagen          ●WiFi │  Title and WiFi indicator (green = on)
├──────────────────────────┤
│   CO2 waveform plot      │  Scrolling CO₂ curve, auto-scaled
├──────────────────────────┤
│  FCO2      4.3  kPa      │  Current CO₂ value
│  O2       20.9  %        │  Oxygen concentration
├──────────────────────────┤
│ [PUMP] [LEAK] [OCCL]     │  Alarm badges: green = OK, red = problem
│ SSID: EAGLEHAGEN         │  WiFi network to join
│ IP: 192.168.4.1          │  Address to open in the browser
│ Out: LabVIEW             │  Current USB output format
└──────────────────────────┘
```

### Alarm badges

| Badge | Red means | What to do |
|---|---|---|
| **PUMP** | The analyser's sample pump is stopped | Press the pump button (IO14) or **Start Pump** on the web page |
| **LEAK** | Leak in the sample line | Check the sample line and its connections |
| **OCCL** | Sample line blocked (occlusion) | Check for kinks, water or a saturated water trap. Then restart the pump. |

---

## 4. Physical buttons

The LilyGO board has two push buttons.

| Button | Action |
|---|---|
| **IO14** (pump button) | Sends **Start Pump** to the CO₂ analyser. Use it after an occlusion or whenever the PUMP badge is red. |
| **BOOT** (GPIO0) | Switches the **USB output format** between **LabVIEW** and **ASCII**. The `Out:` line on the screen shows the current format. |

> ⚠️ Don't hold the **BOOT** button while powering up or resetting the device. That puts the board into firmware-download mode, and the monitor won't start. If this happens, power-cycle without pressing it.

---

## 5. The web page

Open `http://eaglehagen.local` or `http://192.168.4.1` while connected to `EAGLEHAGEN`. Several devices can be connected at once.

> `eaglehagen.local` works on Windows, Mac, iPhone/iPad, Linux and most Android devices. If a device can't find the name, use `192.168.4.1`, which works everywhere.

**Top bar:**
- **Connected / Connecting…**: link to the device. If the link drops, the page reconnects automatically every 3 seconds.
- **Pump / Leak / Occl** badges: the same meaning as on the device screen. They turn red and pulse on a problem (e.g. `Pump!`).

**Value cards:**

| Card | Meaning | Unit |
|---|---|---|
| End-Tidal CO₂ | Peak CO₂ of the last breath | kPa |
| CO₂ | Current CO₂ value | kPa |
| RR | Respiratory rate | breaths/min |
| O₂ | Oxygen concentration | % |
| Volume | Volume sensor reading | mL |

**Charts:** scrolling plots of CO₂, O₂ and Volume covering the last **2 minutes**.

**Buttons:**

| Button | What it does |
|---|---|
| **Start Pump** | Starts the analyser's sample pump |
| **Zero Calibration** | Zero-calibrates the CO₂ analyser. Only do this with **room air (no breath)** in the sample line. |
| **Save Data (CSV)** | Downloads the recorded data as a spreadsheet file |
| **Save Data (JSON)** | Downloads the same data with metadata (for Python/MATLAB) |
| **Clear Data** | Empties the recording and resets the charts |

**USB Host Output Format:** shows and selects the USB output format. It always reflects the device's current setting, including changes made with the BOOT button.

### Recording and exporting data

- **The browser does the recording, not the device.** Recording starts when the page opens and holds the **last 2 minutes** (960 samples). Older samples are dropped.
- The counter under the buttons shows `Data Points: n / 960` and the recording time.
- To capture an event, press **Save** within 2 minutes of it. Keep the page open, because closing or reloading the page loses the data.
- File names look like `medair_co2_data_2026-09-27T10-15-00-000Z.csv`.
- CSV columns: `Timestamp, Elapsed(s), CO2_Waveform(kPa), FetCO2(kPa), FiCO2(kPa), RR(bpm), O2(%), Volume(mL), Pump_Running, Leak_Detected, Occlusion_Detected, Status1, Status2`

For longer recordings, use the USB output (section 6) with a logging program.

---

## 6. Connecting a PC (USB)

1. Connect the device's **USB-C** port to the PC. Use a data cable, not a charge-only one.
2. Find the port:
   - **Windows:** Device Manager → *Ports (COM & LPT)* → "USB Serial Device (COMx)"
   - **Linux:** `/dev/ttyACM0`
3. Open the port with **115200 baud, 8 data bits, no parity, 1 stop bit, no flow control**.
4. Data arrives about **10 times per second** whenever the sensor delivers valid data.

### Output formats (toggle with the BOOT button or the web page)

**LabVIEW (default at power-up):** the format of the original PIC-based system, a mix of text and binary bytes. It's meant for the existing LabVIEW program.

**ASCII (tab-separated):** plain text that's easy to read in a terminal or import into Excel, Python or MATLAB. Each line:

```
CO2_kPa <TAB> O2_% <TAB> RR <TAB> Volume_mL <TAB> Status1 <TAB> Status2
```
Example: `4.3	20.9	14	320	6	0`

> **Diagnostic messages:** in **LabVIEW** format the port carries only data frames. In **ASCII** format the device also sends occasional diagnostic lines, which always start with `#`. A logging program should skip lines starting with `#`.

### Controlling the analyser from the PC

Sending a single byte over the USB port forwards a command to the analyser:

| Byte | Command |
|---|---|
| `0xA5` | Start pump |
| `0x5A` | Zero calibration |

All other bytes are ignored.

---

## 7. Units and normal values

- All CO₂ values are shown in **kPa** (1 kPa ≈ 7.5 mmHg).
- Typical end-tidal CO₂ in a healthy adult is about **4.5–6.0 kPa** (35–45 mmHg).
- Inspired CO₂ should be close to **0**. Raised values suggest rebreathing.
- Room air O₂ is about **20.9 %**.
- The End-Tidal CO₂ value is computed by the monitor from the waveform peak of each breath. It shows `0` until the first breath has been detected.

---

## 8. Troubleshooting

| Problem | Check |
|---|---|
| Screen stays dark | Power supply (+12 V) and the USB-C/5 V feed to the board |
| Stuck on "Connecting sensor…" | It continues after 10 s. If no values appear, check the MaCO2's power and serial cable. |
| No CO₂ values / flat line | Sample line connected? PUMP badge green? Try **Start Pump**. |
| CO₂ reads high with no breath | Run **Zero Calibration** with room air in the sample line |
| OCCL badge red | Kinked or wet sample line or full water trap. Fix, then **Start Pump**. |
| LEAK badge red | Loose connector or damaged sample line |
| Can't find the `EAGLEHAGEN` network | Wait until the main screen appears. Move closer. Power-cycle the device. |
| Web page won't load | Make sure the phone is on `EAGLEHAGEN` (some phones switch back to mobile data when a WiFi has no internet — allow staying connected). Use `http://`, not `https://`. |
| `eaglehagen.local` not found | Use `http://192.168.4.1` instead. Some Android devices and VPN apps don't resolve `.local` names. |
| Page shows "Connecting…" | The device restarted or is out of range. The page reconnects by itself. |
| PC receives no data | Data-capable USB cable? Correct COM port? No other program (e.g. a serial monitor) holding the port? |
| LabVIEW gets garbled data | The format may be set to ASCII. Press BOOT until the screen shows `Out: LabVIEW`. |
| Device doesn't start after pressing BOOT | It's in download mode. Power-cycle without holding BOOT. |

---

## 9. Quick reference

| | |
|---|---|
| WiFi network | `EAGLEHAGEN` (no password) |
| Web address | `http://eaglehagen.local` or `http://192.168.4.1` |
| USB serial | 115200 8N1, ~10 lines/s |
| Pump button | IO14 → Start Pump |
| BOOT button | Toggle USB format LabVIEW ⇄ ASCII |
| Web recording | Last 2 minutes, export CSV/JSON |
| Commands over USB | `0xA5` start pump, `0x5A` zero cal |
