# ESP32 MedAir CO2 Monitor - Software Architecture

## Overview

Modular, class-based architecture for ESP32 ventilator monitoring system. Each major function is encapsulated in its own class for maintainability and testability.

## Class Structure

```
┌─────────────────────────────────────────────────────────────┐
│                        main.cpp                             │
│                   (Application Layer)                       │
│  - Coordinates all modules                                  │
│  - Manages timing and data flow                             │
│  - Handles commands                                         │
└────┬────────┬────────┬────────┬────────┬────────────────────┘
     │        │        │        │        │
     ▼        ▼        ▼        ▼        ▼
┌─────────┐ ┌──────┐ ┌─────────┐ ┌──────┐ ┌──────────┐
│ MaCO2   │ │ ADC  │ │ Display │ │ WiFi │ │   Data   │
│ Parser  │ │ Mgr  │ │ Manager │ │ Mgr  │ │  Logger  │
└─────────┘ └──────┘ └─────────┘ └──────┘ └──────────┘
     │          │          │         │          │
     ▼          ▼          ▼         ▼          ▼
  UART1      GPIO ADC    TFT SPI   WiFi/Web  USB CDC
 (MaCO2)    (Sensors)   (Display)  (Browser) (LabVIEW)
```

## Project Structure

```
ESP32_MedAir/
├── src/
│   ├── main.cpp                 # Main application coordinator
│   ├── MaCO2Parser.h/.cpp       # MaCO2 sensor protocol
│   ├── ADCManager.h/.cpp        # Onboard ADC management
│   ├── DisplayManager.h/.cpp    # TFT display control
│   ├── WiFiManager.h/.cpp       # WiFi & web server (HTML embedded)
│   └── DataLogger.h/.cpp        # LabVIEW output & export
├── platformio.ini               # PlatformIO configuration
└── README.md                    # Project documentation

Note: No data/ folder needed - web interface is embedded in WiFiManager.cpp
```

**Key Design Decision: Embedded Web Interface**

The HTML, CSS, and JavaScript for the web interface are embedded directly in `WiFiManager.cpp` using a raw string literal. This approach:
- ✅ Eliminates need for SPIFFS filesystem
- ✅ Simplifies deployment (single upload)
- ✅ Reduces complexity
- ✅ Faster compilation and flashing
- ✅ No separate file upload step

The complete web dashboard loads from the `getIndexHTML()` function, which returns a ~400-line HTML string containing all markup, styling, and JavaScript.

## Core Classes

### 1. MaCO2Parser
**Purpose:** Handles all communication with MedAir MaCO2-V3 sensor

**Responsibilities:**
- Initialize sensor (handshake protocol)
- Parse 8-byte data packets
- Decode status flags
- Send commands (0xA5, 0x5A)
- Track statistics (packets, errors)

**Key Methods:**
```cpp
bool initialize(HardwareSerial& serial, timeout_ms);
bool parsePacket(HardwareSerial& serial, CO2Data& data);
void sendCommand(HardwareSerial& serial, MaCO2Command cmd);
bool isPumpRunning(const CO2Data& data);
bool isLeakDetected(const CO2Data& data);
bool isOcclusionDetected(const CO2Data& data);
```

**Files:** `MaCO2Parser.h`, `MaCO2Parser.cpp`

---

### 2. ADCManager
**Purpose:** Manages ESP32 onboard ADC for O2 and volume sensors

**Responsibilities:**
- Configure ADC (12-bit, 0-3.3V)
- Apply calibration
- Moving average filtering
- Convert to physical units (%, mL)
- Scale to PIC-compatible format

**Key Methods:**
```cpp
bool begin();
void update(CO2Data& data);
void setO2Calibration(v_at_0, v_at_100);
void setVolumeCalibration(ml_per_volt, offset);
void setFilterSize(uint8_t size);
```

**Files:** `ADCManager.h`, `ADCManager.cpp`

**Features:**
- Configurable moving average filter (default: 10 samples)
- ESP32 ADC calibration support
- PIC format compatibility (16-bit O2, 10-bit Volume)

---

### 3. DisplayManager
**Purpose:** Manages TFT display (LilyGO T-Display S3)

**Responsibilities:**
- Draw waveform (scrolling CO2 trace)
- Display numeric values (EtCO2, FCO2, O2, Volume - RR removed for space)
- Show status indicators (pump, leak, occlusion)
- Backlight control
- Anti-flicker optimization (only update changed values)

**Key Methods:**
```cpp
bool begin();
void showSplash(const char* title, const char* subtitle = nullptr);
void clearScreen();
void drawHeader(bool wifi_connected);
void drawWaveform();
void drawNumericValues(const CO2Data& data);
void drawStatusIndicators(const CO2Data& data);
void updateAll(const CO2Data& data);
void addWaveformPoint(uint16_t co2_value);
void setBacklight(uint8_t brightness);
```

**Files:** `DisplayManager.h`, `DisplayManager.cpp`

**Display Layout (170x320):**
```
┌─────────────────────────────┐
│   Ornhagen             ●    │  Header (25px)
│                             │  Centered text, WiFi indicator
├─────────────────────────────┤
│                             │
│  CO2 Waveform (scrolling)   │  Waveform (130px)
│  Soft blue line with grid   │  8Hz sampling
│                             │
├─────────────────────────────┤
│ ┌──────────┐ ┌──────────┐  │
│ │  EtCO2   │ │  FCO2    │  │  Values (100px)
│ │  38.5    │ │   0.2    │  │  Color-coded by type
│ └──────────┘ └──────────┘  │
│ ┌──────────┐ ┌──────────┐  │
│ │    O2    │ │  Volume  │  │
│ │   21.0   │ │   450    │  │
│ └──────────┘ └──────────┘  │
├─────────────────────────────┤
│ [PUMP][LEAK][OCCL]          │  Status (65px)
│ ────────────────            │  Network info
│ SSID: EAGLEHAGEN            │
│ IP: 192.168.4.1             │
└─────────────────────────────┘
```

**Design Features:**
- Soft color palette (medical-grade appearance)
- FreeSansBold font for header
- Text-only splash screens (no bitmap logo)
- Optimized layout: 25/130/100/65 pixel sections
- Anti-flicker updates (only changed values)
- Color-coded values (darker blue for CO2, lighter for O2/Volume)
- Bright green WiFi indicator when connected

---

### 4. WiFiManager
**Purpose:** Manages WiFi connection and web server

**Responsibilities:**
- WiFi AP or Station mode
- Async web server (ESPAsyncWebServer)
- WebSocket for real-time updates
- REST API for data/commands
- Command queue from web interface
- **Embedded web interface** (HTML/CSS/JavaScript in code)

**Key Methods:**
```cpp
bool beginAP(ssid, password);
bool beginStation(ssid, password, timeout);
bool startServer();
void update(const CO2Data& data);  // Broadcasts to WebSocket
bool hasCommand();
uint8_t getCommand();
```

**Files:** `WiFiManager.h`, `WiFiManager.cpp`

**Web Interface:**
- Complete HTML, CSS, and JavaScript embedded in `getIndexHTML()`
- Örnhagen logo (WebP format, 10KB embedded as base64)
- No separate data/ folder or SPIFFS required
- Chart.js loaded from CDN
- Single-file deployment for simplicity
- Professional branding with logo left of title

**Endpoints:**
- `GET /` - Web dashboard (HTML + logo embedded in code)
- `WS /ws` - WebSocket (real-time updates at 8 Hz)

---

### 5. DataLogger
**Purpose:** Handles LabVIEW compatibility and data export

**Responsibilities:**
- Format data in PIC-compatible format
- Send to USB CDC (Virtual COM port)
- Handle zero replacement
- Track statistics
- Future: CSV logging

**Key Methods:**
```cpp
bool begin();
void sendPICFormat(Stream& stream, const CO2Data& data);
void setLabViewEnabled(bool enabled);
```

**Files:** `DataLogger.h`, `DataLogger.cpp`

**PIC Format:**
```
<ESC>ABC<TAB>DEFGH<TAB>IJKLM<TAB>[Status1][Status2][RR][FCO2][FetCO2]<CR><LF>
```

---

## Data Flow

```
┌──────────┐
│  MaCO2   │ ──8 bytes──> MaCO2Parser ──┐
│  Sensor  │                             │
└──────────┘                             │
                                         ▼
┌──────────┐                        ┌────────────┐
│ O2 Sensor│ ──ADC──> ADCManager ──>│  CO2Data   │
└──────────┘                        │  (struct)  │
┌──────────┐                        └─────┬──────┘
│Vol Sensor│ ──ADC──> ADCManager ──>      │
└──────────┘                              │
                                          │
        ┌─────────────────────────────────┼────────────────┐
        │                                 │                │
        ▼                                 ▼                ▼
┌──────────────┐                  ┌─────────────┐  ┌────────────┐
│DisplayManager│                  │WiFiManager  │  │ DataLogger │
│  (TFT)       │                  │(WebSocket)  │  │ (USB CDC)  │
└──────────────┘                  └─────────────┘  └────────────┘
        │                                 │                │
        ▼                                 ▼                ▼
   User sees                         Browser         LabVIEW
  waveforms/                         clients         receives
   values                                             PIC format
```

## Timing

**Update Rates:**
- **Data Acquisition:** 8 Hz (125ms) - Parse MaCO2, read ADC
- **Display Refresh:** 20 Hz (50ms) - Update TFT display
- **WiFi Broadcast:** 8 Hz (125ms) - Send to WebSocket clients (matches data rate)
- **LabVIEW Output:** 8 Hz (125ms) - Send PIC format to USB CDC

**Data Buffering:**
- Web interface stores last 960 data points (2 minutes at 8 Hz)
- Automatic scrolling as new data arrives
- Export capability for complete dataset

**Non-blocking Design:**
All operations are non-blocking. No delays in main loop (except 1ms for watchdog).

## Configuration

### Pin Assignments
```cpp
#define UART_RX_MACO2   44  // MaCO2 sensor RX
#define UART_TX_MACO2   43  // MaCO2 sensor TX
#define O2_SENSOR_PIN   1   // ADC1_CH0
#define VOL_SENSOR_PIN  2   // ADC1_CH1
```

### WiFi Settings
```cpp
const char* WIFI_SSID = "MedAir_Monitor";
const char* WIFI_PASSWORD = "co2monitor";
const bool WIFI_MODE_AP = true;  // AP or Station
```

### Calibration
```cpp
// O2 sensor: voltage at 0% and 100%
adcManager.setO2Calibration(0.0, 3.3);

// Volume sensor: mL per volt, offset
adcManager.setVolumeCalibration(200.0, 0.0);
```

## Building and Flashing

### PlatformIO (Recommended)
```ini
[env:lilygo-t-display-s3]
platform = espressif32
board = lilygo-t-display-s3
framework = arduino

lib_deps = 
    TFT_eSPI
    ESPAsyncWebServer
    AsyncTCP
    ArduinoJson

build_flags = 
    ; TFT_eSPI configuration for T-Display S3
    -DUSER_SETUP_LOADED=1
    -DST7789_DRIVER=1
    -DTFT_WIDTH=170
    -DTFT_HEIGHT=320
    ; Add other flags as needed
```

**No filesystem configuration needed** - web interface is embedded in code.

### Arduino IDE
1. Install ESP32 board support
2. Select "LilyGO T-Display S3"
3. Install libraries: TFT_eSPI, ESPAsyncWebServer, AsyncTCP, ArduinoJson
4. Configure TFT_eSPI User_Setup for T-Display S3
5. Compile and upload

## Usage

### For Standalone Operation
1. Power on ESP32
2. Connect to WiFi "MedAir_Monitor" (password: "co2monitor")
3. Open browser to http://192.168.4.1
4. Monitor data in real-time

### For LabVIEW Integration
1. Connect ESP32 to PC via USB-C
2. Open Device Manager (Windows) to find COM port
3. Configure LabVIEW VISA:
   - Port: COMx (e.g., COM3)
   - Baud: 115200 (convention, doesn't matter for USB CDC)
   - Data: 8N1
4. LabVIEW receives PIC-compatible data stream
5. **No code changes needed in LabVIEW!**

### Commands
From web interface or LabVIEW:
- `0xA5` - Start pump
- `0x5A` - Zero calibration

## Testing

### Serial Monitor
Connect USB and open Serial Monitor (115200 baud) to see:
- Initialization messages
- Packet statistics
- Alarm notifications
- Debug output

### Test Commands
```cpp
// Via Serial Monitor, send:
0xA5  // Start pump
0x5A  // Zero calibration
```

## Future Enhancements

1. **SD Card Logging:** Store data to microSD
2. **MQTT Support:** Publish to hospital monitoring systems
3. **Calibration UI:** Web-based calibration interface
4. **Trend Analysis:** Historical data visualization
5. **Multi-patient:** Support multiple sensors

## Troubleshooting

**MaCO2 not initializing:**
- Check UART connections (TX/RX not swapped?)
- Verify level shifter working
- Check +12V power to sensor
- System will continue and retry connection

**ADC readings incorrect:**
- Verify voltage dividers (if using 5V sensors)
- Check calibration values
- Use `printStatus()` to see raw ADC values

**Display not working:**
- Check TFT_eSPI configuration
- Verify SPI pins in User_Setup.h
- Test with TFT_eSPI examples first

**WiFi not connecting:**
- Check SSID/password
- Try AP mode instead of Station
- Verify ESP32 not in boot mode

**LabVIEW not receiving data:**
- Verify USB-C cable (data capable, not charge-only)
- Check COM port in Device Manager
- Try different baud rate (115200 recommended)
- Verify `dataLogger.setLabViewEnabled(true)`

## License

[Your license here]

## Author

Åke - Embedded systems engineer
