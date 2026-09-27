# Eaglehagen CO₂ Monitor

ESP32-S3 (LilyGO T-Display S3) replacement for a legacy PIC16F876A/LabVIEW setup. It reads a MedAir MaCO2-V3 side-stream CO₂ analyser plus O₂ and volume sensors, and shows the data on the built-in screen, on a phone or tablet over WiFi, and as a serial stream over USB.

| Document | For |
|---|---|
| [USER_GUIDE.md](USER_GUIDE.md) | Operating the device: screen, buttons, web page, data export, PC connection, troubleshooting |
| [documentation/DESIGN_REFERENCE.md](documentation/DESIGN_REFERENCE.md) | Hardware, sensor protocol, firmware architecture, interfaces, build setup, history |
| [tools/Eaglehagen_Serial_Monitor.html](tools/Eaglehagen_Serial_Monitor.html) | Stand-alone PC monitor over USB: accepts both output formats (LabVIEW and ASCII), records to CSV. Also downloadable from the device's web page (embedded in the firmware at build time). Open in Chrome/Edge; add `#demo` for synthetic data |

**Quick start:** power on → join WiFi `EAGLEHAGEN` (no password) → open `http://eaglehagen.local` (or `http://192.168.4.1`).

**Build:** `pio run -t upload` (PlatformIO). No library edits are needed; the display setup is in [include/TFT_eSPI_Setup.h](include/TFT_eSPI_Setup.h).
