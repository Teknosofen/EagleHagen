# ESP32 MedAir CO2 Monitor

Modern replacement for legacy PIC16F876A-based MedAir MaCO2-V3 ventilator monitoring system.

## Quick Start

### 1. Hardware Required
- LilyGO T-Display S3 (ESP32-S3)
- MedAir MaCO2-V3 CO2 sensor
- O2 sensor (analog output)
- Volume sensor (analog output)
- 7805 voltage regulator (+12V to +5V)
- Level shifter components (1kΩ resistor + 3.3V zener diode)

### 2. Install PlatformIO
```bash
# Install PlatformIO Core
pip install platformio

# Or use PlatformIO IDE (VSCode extension)
```

### 3. Clone and Build
```bash
# Create project directory
mkdir ESP32_MedAir
cd ESP32_MedAir

# Initialize PlatformIO project
pio project init --board lilygo-t-display-s3

# Copy all source files to src/ directory
# - main.cpp
# - MaCO2Parser.h/.cpp
# - ADCManager.h/.cpp
# - DisplayManager.h/.cpp
# - WiFiManager.h/.cpp
# - DataLogger.h/.cpp

# Copy platformio.ini to project root

# Build
pio run

# Upload
pio run --target upload

# Monitor serial output
pio device monitor
```

### 4. Connect and Use

#### Option A: Standalone WiFi Monitoring
1. Power on ESP32
2. Connect to WiFi AP: `EAGLEHAGEN` (password: `co2monitor`)
3. Open browser: `http://192.168.4.1`
4. View real-time data, charts, and export data

#### Option B: LabVIEW Integration
1. Connect ESP32 to PC via USB-C
2. Open Device Manager to find COM port
3. Configure LabVIEW VISA: 115200 baud, 8N1
4. LabVIEW receives PIC-compatible data stream
5. **No LabVIEW code changes needed!**

#### Option C: Both Simultaneously
- Web interface via WiFi for monitoring
- LabVIEW via USB for data logging/analysis

## Project Structure

```
ESP32_MedAir/
├── src/
│   ├── main.cpp                 # Main application coordinator
│   ├── MaCO2Parser.h/.cpp       # MaCO2 sensor communication
│   ├── ADCManager.h/.cpp        # ESP32 ADC management
│   ├── DisplayManager.h/.cpp    # TFT display control
│   ├── WiFiManager.h/.cpp       # WiFi & web server (HTML embedded)
│   └── DataLogger.h/.cpp        # LabVIEW output & data export
├── platformio.ini               # PlatformIO configuration
├── LIBRARY_DEPENDENCIES.md      # Library installation guide
├── SOFTWARE_ARCHITECTURE.md     # Detailed architecture docs
└── README.md                    # This file
```

**Important:** No `data/` folder needed - the web interface is embedded in `WiFiManager.cpp`!

## Features

### TFT Display (170x320)
```
┌─────────────────────────────┐
│   Ornhagen             ●    │  Header (25px, centered, WiFi)
├─────────────────────────────┤
│  CO2 Waveform (scrolling)   │  Real-time waveform
│  ╱╲  ╱╲    ╱╲  ╱╲          │  (130px, soft blue)
├─────────────────────────────┤
│ EtCO2  FCO2                 │  Numeric values
│ 38mmHg  0mmHg    (no RR)    │  (100px, with
│ O2      Volume               │   decimals and
│ 21.0%   450mL                │   units)
├─────────────────────────────┤
│ [●PUMP] [●OK] [○LEAK]       │  Status indicators
│ SSID: EAGLEHAGEN            │  (65px, network info)
│ IP: 192.168.4.1             │
└─────────────────────────────┘
```

### Web Interface
- 🎨 Örnhagen logo branding (WebP, left of title)
- 📊 Three real-time scrolling charts (CO2, O2, Volume)
- 📈 Instant value displays
- 🎛️ Control buttons (Start Pump, Zero Calibration)
- 💾 Data export (CSV/JSON with 2-minute buffer)
- 🚦 Status indicators (Pump, Leak, Occlusion)
- 📱 Responsive design (works on any device)

### Data Acquisition
- ⚡ 8 Hz sampling rate (125ms intervals)
- 🔄 Real-time processing
- 📡 WebSocket broadcast to web clients
- 🔌 USB CDC output for LabVIEW
- 💾 2-minute circular buffer (960 points)

### LabVIEW Compatibility
- ✅ Identical data format to original PIC system
- ✅ USB CDC Virtual COM port
- ✅ Zero code changes required in LabVIEW
- ✅ Just change COM port number

## Configuration

### WiFi Settings (main.cpp)
```cpp
const char* WIFI_SSID = "EAGLEHAGEN";
const char* WIFI_PASSWORD = "co2monitor";
const bool WIFI_AP_MODE = true;  // true = AP, false = Station
```

### Pin Definitions (main.cpp)
```cpp
#define UART_RX_MACO2   44   // MaCO2 sensor RX
#define UART_TX_MACO2   43   // MaCO2 sensor TX
#define O2_SENSOR_PIN   1    // ADC1_CH0
#define VOL_SENSOR_PIN  2    // ADC1_CH1
```

### Sensor Calibration (main.cpp)
```cpp
// O2 sensor: voltage at 0% and 100%
adcManager.setO2Calibration(0.0, 3.3);

// Volume sensor: mL per volt, offset
adcManager.setVolumeCalibration(200.0, 0.0);
```

### Update Rates (main.cpp)
```cpp
#define DATA_UPDATE_INTERVAL_MS     125   // 8Hz data acquisition
#define DISPLAY_UPDATE_INTERVAL_MS  50    // 20Hz display refresh
#define WIFI_UPDATE_INTERVAL_MS     125   // 8Hz web update
#define LABVIEW_UPDATE_INTERVAL_MS  125   // 8Hz LabVIEW output
```

## Libraries Required

Install via PlatformIO Library Manager or platformio.ini:

### Available in PlatformIO Registry:
- **TFT_eSPI** (bodmer/TFT_eSPI) - Display driver
- **ArduinoJson** (bblanchon/ArduinoJson) - JSON parsing

### Install from GitHub:
- **ESPAsyncWebServer** - https://github.com/me-no-dev/ESPAsyncWebServer.git
- **AsyncTCP** - https://github.com/me-no-dev/AsyncTCP.git

See `LIBRARY_DEPENDENCIES.md` for detailed installation instructions.

## Hardware Connections

### Power Supply
```
+12V Input ──> 7805 ──> +5V ──> ESP32 USB-C
               [Caps]
```

### MaCO2 Sensor
```
MaCO2 TX (5V) ──> 1kΩ + 3.3V Zener ──> ESP32 GPIO 44 (RX)
MaCO2 RX (5V) <──────────────────────── ESP32 GPIO 43 (TX)
```

### Analog Sensors
```
O2 Sensor ──> [Voltage Divider if 5V] ──> ESP32 GPIO 1 (ADC)
Volume Sensor ──> [Voltage Divider if 5V] ──> ESP32 GPIO 2 (ADC)
```

Note: If sensors output 5V, use voltage dividers (10kΩ + 20kΩ) to scale to 3.3V

## Web Interface Usage

### Accessing the Dashboard
1. Connect to WiFi AP: `MedAir_Monitor`
2. Open browser: `http://192.168.4.1`

### Features

#### Real-time Monitoring
- Three scrolling charts show last 2 minutes of data
- Instant value displays update at 8 Hz
- Status badges show pump/leak/occlusion state

#### Data Export
- **Save Data (CSV)** - Excel-compatible format
  - Columns: Timestamp, CO2, O2, Volume, Status, etc.
  - Filename: `medair_co2_data_YYYY-MM-DD-HH-MM-SS.csv`
  
- **Save Data (JSON)** - Structured format with metadata
  - Includes: timestamps, sample rate, duration
  - Perfect for Python/MATLAB analysis
  - Filename: `medair_co2_data_YYYY-MM-DD-HH-MM-SS.json`

#### Control Buttons
- **▶️ Start Pump** - Sends 0xA5 command to MaCO2
- **🎯 Zero Calibration** - Sends 0x5A command to MaCO2
- **🗑️ Clear Data** - Clears buffer and resets charts

## LabVIEW Integration

### Setup
1. Connect ESP32 to PC via USB-C
2. Windows: Device Manager → Ports → Note COM port number
3. Linux: Appears as `/dev/ttyACM0`

### LabVIEW Configuration
```
Port: COMx (e.g., COM3)
Baud Rate: 115200
Data Bits: 8
Parity: None
Stop Bits: 1
Flow Control: None
```

### Data Format
Identical to original PIC output:
```
<ESC>ABC<TAB>DEFGH<TAB>IJKLM<TAB>[Status1][Status2][RR][FCO2][FetCO2]<CR><LF>
```
Where:
- ABC = CO2 waveform (3 digits)
- DEFGH = O2 ADC (5 digits, 0-65535)
- IJKLM = Volume ADC (5 digits, 0-1023)
- Status bytes and values

**No LabVIEW code changes required!**

## Troubleshooting

### Display not working
- Check TFT_eSPI configuration in `platformio.ini`
- Verify pin definitions match T-Display S3
- Test with TFT_eSPI examples first

### MaCO2 not initializing
- Check UART connections (TX/RX not swapped?)
- Verify level shifter circuit
- Check +12V power to sensor
- System continues and retries automatically

### ADC readings incorrect
- Verify voltage dividers (if using 5V sensors)
- Check calibration values in `main.cpp`
- Use Serial Monitor to view raw ADC readings

### WiFi not working
- Check SSID/password in `main.cpp`
- Verify ESP32 is not in boot mode
- Try different device to connect

### LabVIEW not receiving data
- Verify USB cable is data-capable (not charge-only)
- Check COM port in Device Manager
- Confirm `dataLogger.setLabViewEnabled(true)` in code

## Serial Monitor Commands

Connect at 115200 baud to see:
- Initialization messages
- Packet statistics
- Error notifications
- Debug output

## Performance

### Memory Usage
- Flash: ~200KB (plenty of room in 16MB)
- RAM: ~50-60KB (plenty of room in 512KB)
- No SPIFFS filesystem needed

### Update Rates
- MaCO2 sensor: 8 Hz
- ADC sampling: 8 Hz
- Display refresh: 20 Hz
- WiFi broadcast: 8 Hz
- LabVIEW output: 8 Hz

### Network
- WebSocket: ~8KB/sec at 8 Hz
- Supports multiple simultaneous clients
- Auto-reconnect on disconnect

## Development

### Adding Features
1. Modify source files in `src/`
2. Rebuild: `pio run`
3. Upload: `pio run --target upload`

### Customizing Web Interface
- Edit `getIndexHTML()` function in `WiFiManager.cpp`
- All HTML/CSS/JavaScript is in one place
- Rebuild and upload

### Calibration
- Edit sensor calibration in `setup()` function in `main.cpp`
- Rebuild and upload

## Documentation

- **SOFTWARE_ARCHITECTURE.md** - Detailed software design
- **LIBRARY_DEPENDENCIES.md** - Library installation guide
- **ESP32_MedAir_Implementation.md** - Hardware design guide
- **CO2_Analyzer_Serial_Interface.md** - Protocol documentation

## License

[Your license here]

## Author

Åke - Embedded Systems Engineer
- Experience: ESP32, motor control, medical devices
- Location: Södertälje, Stockholm, SE

## Support

For questions or issues:
1. Check documentation files
2. Review Serial Monitor output (115200 baud)
3. Verify hardware connections
4. Check library versions

## Version History

**v1.0** - Initial release
- Complete PIC replacement
- TFT display with waveform
- Web interface with real-time charts
- LabVIEW compatibility
- Data export (CSV/JSON)
- WiFi AP mode
- USB CDC for LabVIEW

## Future Enhancements

- SD card logging
- MQTT support
- WiFi configuration portal
- Multiple sensor support
- Cloud connectivity
- Historical data analysis
