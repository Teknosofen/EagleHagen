# Eaglehagen CO₂ Monitor — User Guide

This guide explains how to use the monitor day to day. For wiring, firmware and protocol details, see [documentation/DESIGN_REFERENCE.md](documentation/DESIGN_REFERENCE.md).

---

## 1. What the device does

The Eaglehagen monitor connects to a **MedAir MaCO2-V3** side-stream CO₂ analyser, an **O₂ sensor** and a **volume sensor**. It shows the readings in three places:

| Where | What you get |
|---|---|
| **Built-in screen** | CO₂ waveform, current CO₂ (kPa), O₂ (%), alarm badges, network info |
| **Phone / tablet / laptop over WiFi** | Live values, three scrolling charts, pump/calibration buttons, data export (CSV/JSON) |
| **PC over USB** | Continuous serial data stream for LabVIEW, the PC monitor page (section 7, downloadable from the device), or any terminal/logging program |

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

**PC monitor page (USB):** the **Download** button at the bottom saves the stand-alone PC monitor page (`Eaglehagen_Serial_Monitor.html`) to the computer you're browsing from. See section 7.

### Recording and exporting data

- **The browser does the recording, not the device.** Recording starts when the page opens and holds the **last 2 minutes** (960 samples). Older samples are dropped.
- The counter under the buttons shows `Data Points: n / 960` and the recording time.
- To capture an event, press **Save** within 2 minutes of it. Keep the page open, because closing or reloading the page loses the data.
- File names look like `medair_co2_data_2026-09-27T10-15-00-000Z.csv`.
- CSV columns: `Timestamp, Elapsed(s), CO2_Waveform(kPa), FetCO2(kPa), FiCO2(kPa), RR(bpm), O2(%), Volume(mL), Pump_Running, Leak_Detected, Occlusion_Detected, Status1, Status2`

For longer recordings, use the PC monitor page over USB (section 7): it records without a time limit.

---

## 6. Connecting a PC (USB)

1. Connect the device's **USB-C** port to the PC. Use a data cable, not a charge-only one.
2. Find the port:
   - **Windows:** Device Manager → *Ports (COM & LPT)* → "USB Serial Device (COMx)" (Swedish Windows: "Seriell USB-enhet")
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

> **Tip:** the PC monitor page (section 7) accepts both formats and detects which one is in use.

> **Diagnostic messages:** in **LabVIEW** format the port carries only data frames. In **ASCII** format the device also sends occasional diagnostic lines, which always start with `#`. A logging program should skip lines starting with `#`.

### Controlling the analyser from the PC

Sending a single byte over the USB port forwards a command to the analyser:

| Byte | Command |
|---|---|
| `0xA5` | Start pump |
| `0x5A` | Zero calibration |

All other bytes are ignored.

---

## 7. PC monitor page (no LabVIEW needed)

`Eaglehagen_Serial_Monitor.html` is a stand-alone page that shows the monitor's data on a PC over the USB cable. It needs no installation and no internet, and nothing is sent anywhere.

The monitor itself carries a copy of the page, so you don't need any other files. You can also find it in this project as [tools/Eaglehagen_Serial_Monitor.html](tools/Eaglehagen_Serial_Monitor.html).

> **Both output formats work.** The page reads the **LabVIEW** format and the **ASCII** format, and detects which one is arriving by itself. You don't need to press BOOT before using it. If someone presses BOOT while you are connected, the page follows the switch without reconnecting.

### Step 1: Get the page over WiFi (once per PC)

1. On the PC, join the WiFi network **`EAGLEHAGEN`** (no password).
2. Open **`http://eaglehagen.local`** (or `http://192.168.4.1`).
3. At the bottom of the web page, under **PC monitor page (USB)**, click **Download**. Save the file somewhere easy to find, for example the desktop.
4. Switch the PC back to its normal WiFi if you like. The saved page doesn't need any network.

The page can't be used directly from the WiFi address: browsers only allow serial-port access from a saved file. The saved copy keeps working, so this step is needed only once per PC (and again after a firmware update, to get the latest version).

### Step 2: Connect with the USB cable

1. Close LabVIEW, or any other program using the monitor's COM port. Only one program can use a port at a time.
2. Connect the monitor's USB-C port to the PC.
3. Open the saved file in **Google Chrome** or **Microsoft Edge** (right-click → *Open with*, if another browser is the default). Other browsers can't access serial ports; the page tells you if that's the problem.
4. Scroll to the **Serial port** panel at the bottom of the page.
   - **First time only:** click **Add port…** and choose the monitor's COM port in the browser's list. It may be named "USB JTAG/serial debug unit" or "USB Serial Device". The browser requires this one step.
   - After that, the monitor appears in the **Serial port** list and is selected automatically.
5. Leave **Baud rate** at 115200 and **Data format** at *Auto-detect*, then click **Connect selection**.

The status pill at the top right shows **Receiving data** when values are coming in.

### What you see

- **Alarm badges:** Data valid, Pump, Leak and Occlusion. Green means OK; red and pulsing means a problem.
- **Values:** end-tidal CO₂ (kPa, with mmHg underneath), CO₂, respiratory rate, O₂ and volume.
- **Charts:** scrolling CO₂ waveform, O₂ and volume. The time window can be set to 30 s, 1, 2 or 5 minutes.
- **◐ button:** switches between a dark and a light theme.

What differs between the two formats:

| | LabVIEW format | ASCII format |
|---|---|---|
| End-tidal CO₂ | Sent by the monitor | Calculated by the page from the waveform |
| FiCO₂ | Shown under the CO₂ value | Not available |
| Volume | Raw ADC counts (0–1023) | mL |
| Device messages (`#` lines) | None | Shown in the log at the bottom |

### Recording and CSV

- Everything received while connected is recorded, with **no 2-minute limit**. **Pause** stops recording temporarily and **Clear** starts over.
- **Save CSV** stores the recording. Chrome and Edge let you choose where to save it.
- **CSV style:** choose *Swedish Excel* if the file will be opened in Excel with Swedish settings (it then uses `;` and decimal commas).
- The columns are the same as the web page export, plus `Volume(ADC)` and `Format`. CO₂ values are in kPa.

### Other controls

- **Start pump** and **Zero calibration** work as on the web page. Zero calibration asks for confirmation; use it only with room air.
- **Device messages & connection log** at the bottom lists connections, format changes and device messages.
- To try the page without a device, add `#demo` to the end of the address in the browser.

---

## 8. Units and normal values

- All CO₂ values are shown in **kPa** (1 kPa ≈ 7.5 mmHg).
- Typical end-tidal CO₂ in a healthy adult is about **4.5–6.0 kPa** (35–45 mmHg).
- Inspired CO₂ should be close to **0**. Raised values suggest rebreathing.
- Room air O₂ is about **20.9 %**.
- The End-Tidal CO₂ value is computed by the monitor from the waveform peak of each breath. It shows `0` until the first breath has been detected.

---

## 9. Troubleshooting

| Problem | Check |
|---|---|
| Screen stays dark | Power supply (+12 V) and the USB-C/5 V feed to the board |
| Stuck on "Connecting sensor…" | It continues after 10 s. If no values appear, check the MaCO2's power and serial cable. |
| No CO₂ values / flat line | Sample line connected? PUMP badge green? Try **Start Pump**. |
| CO₂ reads high with no breath | Run **Zero Calibration** with room air in the sample line |
| OCCL badge red | Kinked or wet sample line or full water trap. Fix, then **Start Pump**. |
| LEAK badge red | Loose connector or damaged sample line |
| Can't find the `EAGLEHAGEN` network | Wait until the main screen appears. Move closer. Power-cycle the device. |
| No **Download** button on the web page | Reload with Ctrl+F5 (the browser may show an old cached page). The button needs the current firmware. |
| Web page won't load | Make sure the phone is on `EAGLEHAGEN` (some phones switch back to mobile data when a WiFi has no internet — allow staying connected). Use `http://`, not `https://`. |
| `eaglehagen.local` not found | Use `http://192.168.4.1` instead. Some Android devices and VPN apps don't resolve `.local` names. |
| Page shows "Connecting…" | The device restarted or is out of range. The page reconnects by itself. |
| PC receives no data | Data-capable USB cable? Correct COM port? No other program (e.g. a serial monitor) holding the port? |
| LabVIEW gets garbled data | The format may be set to ASCII. Press BOOT until the screen shows `Out: LabVIEW`. |
| PC monitor page can't connect | Close LabVIEW or other programs using the COM port. Use Chrome or Edge. |
| PC monitor page says the browser can't open serial ports | You opened it from the WiFi address, or in another browser. Download it, then open the saved file in Chrome or Edge. |
| Device doesn't start after pressing BOOT | It's in download mode. Power-cycle without holding BOOT. |

---

## 10. Quick reference

| | |
|---|---|
| WiFi network | `EAGLEHAGEN` (no password) |
| Web address | `http://eaglehagen.local` or `http://192.168.4.1` |
| USB serial | 115200 8N1, ~10 lines/s |
| Pump button | IO14 → Start Pump |
| BOOT button | Toggle USB format LabVIEW ⇄ ASCII |
| Web recording | Last 2 minutes, export CSV/JSON |
| Commands over USB | `0xA5` start pump, `0x5A` zero cal |
| PC monitor page | Download from the web page (WiFi), then open the file in Chrome/Edge; both formats |
