# ESP32 MedAir CO2 Monitor - Modern Implementation

## Project Overview

Replace legacy LabVIEW/PC system with LilyGO T-Display S3 ESP32 while maintaining backward compatibility with existing PIC16F876A interface and MedAir MaCO2-V3 protocol.

## Hardware: LilyGO T-Display S3

**Specifications:**
- ESP32-S3 dual-core processor
- 1.9" ST7789V TFT display (170x320 pixels)
- Built-in RGB LED
- USB-C for programming and power
- Multiple UART interfaces
- WiFi/Bluetooth capability

**Key Advantages:**
- Built-in display for local monitoring
- Web server capability for remote access
- Fast enough for real-time waveform processing
- Low power consumption
- Compact form factor

## System Architecture

### Current PIC System (Legacy)

```
                    ┌─────────────────────┐
          ┌────────>│  MedAir MaCO2-V3   │
          │  TX     │   CO2 Analyzer     │
    Host/LabVIEW    │                    │
          │         └──────────┬──────────┘
          │                    │ TX (data out)
          │                    │ 9600 baud, 8N1
          │         ┌──────────▼──────────┐
          │         │  PIC16F876A        │◄─── AN0 (O2 sensor)
          │  RX     │  RX: From MaCO2    │◄─── AN1 (Volume sensor)
          └─────────┤  TX: To Host       │
           (receives│                    │
            enhanced│  Host TX bypasses  │
            data)   │  PIC completely    │
                    └────────────────────┘

Signal Flow (Split Serial):
1. Host TX ──────────→ MaCO2 RX (direct, bypasses PIC)
2. MaCO2 TX ─────────→ PIC RX (8 bytes data)
3. PIC samples ADCs (AN0, AN1)
4. PIC combines MaCO2 data + ADC readings
5. PIC TX ───────────→ Host RX (enhanced packet)
```

**Key Points - Current System:**
- **Commands:** Host TX → MaCO2 RX (direct, PIC not involved)
- **Data:** MaCO2 TX → PIC RX → PIC processes → PIC TX → Host RX
- PIC has **no visibility** of commands (they bypass it completely)
- This is a "split serial" configuration
- PIC only handles data reception and enhancement

### New ESP32 System (Proposed)

```
                    ┌─────────────────────┐
          ┌────────>│  MedAir MaCO2-V3   │
          │  TX     │   CO2 Analyzer     │
          │  (cmds) │                    │
          │         └──────────┬──────────┘
          │                    │ TX (data out)
          │                    │ 9600 baud, 8N1
          │         ┌──────────▼──────────┐
          │         │  ESP32-S3          │◄─── O2 sensor (GPIO1 ADC)
          │  RX     │  (T-Display S3)    │◄─── Vol sensor (GPIO2 ADC)
          └─────────┤                    │
         (data)     │  UART1: MaCO2      │
                    │  ┌──────────────┐  │
                    │  │ TFT Display  │  │
                    │  │  170x320     │  │
                    │  └──────────────┘  │
                    │                    │
                    │  USB-C ◄──────────┼──► PC/LabVIEW
                    │  (Virtual COM)     │     (data + commands)
                    │                    │
                    │  WiFi ◄───────────┼──► Browser Clients
                    │                    │     (web interface)
                    └────────────────────┘

Signal Flow:
1. LabVIEW/Web → ESP32 (via USB-C or WiFi)
2. ESP32 TX ──────────→ MaCO2 RX (commands: 0xA5, 0x5A)
3. MaCO2 TX ──────────→ ESP32 RX (8 bytes data)
4. ESP32 reads onboard ADCs (O2, Volume)
5. ESP32 combines all data
6. ESP32 → LabVIEW/Web (USB-C or WiFi, enhanced packet)
```

**Key Points - New System:**
- ESP32 has **full bidirectional** UART to MaCO2
- Commands fully visible and logged by ESP32
- Uses **onboard 12-bit ADC** (no external ADC needed)
- **USB-C** provides virtual COM port for LabVIEW
- **WiFi** provides web interface access
- Single device replaces PIC + PC/LabVIEW setup

## Web Interface

### Embedded HTML Dashboard

The web interface is **completely embedded** in `WiFiManager.cpp` - no separate files needed!

**Access:** Connect to WiFi AP "MedAir_Monitor" and open http://192.168.4.1

**Features:**
- ✅ **Real-time Monitoring** - Updates at 8 Hz via WebSocket
- ✅ **Three Scrolling Charts:**
  - CO2 Waveform (mmHg)
  - O2 Level (%)
  - Volume (mL)
- ✅ **Instant Values Display:**
  - End-Tidal CO2 (FetCO2)
  - Inspired CO2 (FiCO2)
  - Respiratory Rate (RR)
  - O2 Percentage
  - Volume
- ✅ **Status Indicators:**
  - Pump Running/Stopped
  - Leak Detected/OK
  - Occlusion Detected/OK
- ✅ **Control Buttons:**
  - Start Pump (sends 0xA5)
  - Zero Calibration (sends 0x5A)
- ✅ **Data Export:**
  - Save to CSV (Excel-compatible)
  - Save to JSON (with metadata)
  - 2-minute buffer (960 points at 8 Hz)
  - Clear data button
- ✅ **Responsive Design** - Works on phone, tablet, desktop

**Implementation Details:**
- HTML, CSS, and JavaScript embedded in `getIndexHTML()` function
- Chart.js loaded from CDN (https://cdn.jsdelivr.net/npm/chart.js)
- No SPIFFS filesystem required
- Single-file deployment
- ~400 lines of embedded web code

**Data Flow:**
```
ESP32 (8Hz) → WebSocket → Browser
Browser → WebSocket → ESP32 (commands)
```

## Hardware Design

### Power Supply

```
+12V Input ──┬──> MaCO2 Sensor (+12V required)
             │
             └──> 7805 Voltage Regulator ──> +5V ──┬──> USB-C (ESP32 power)
                  (with 100nF input/output caps)    │
                                                     └──> External ADC VCC
```

**Components:**
- **7805** Linear regulator (TO-220 package)
  - Input: +12V (7V-35V range)
  - Output: +5V @ 1A max
  - Add heatsink if total current > 500mA
  - **Input cap:** 100nF ceramic + 10µF electrolytic
  - **Output cap:** 100nF ceramic + 10µF electrolytic

**Power Budget:**
- MaCO2 sensor: ~200-300mA @ 12V (estimated)
- ESP32-S3: ~200-300mA @ 5V (via USB-C)
- TFT backlight: ~100mA @ 5V
- External ADC: ~1-2mA @ 5V
- **Total @ 5V:** ~300-400mA (well within 7805 capacity)

**Alternative:** If 7805 gets too hot, use a buck converter (LM2596) for better efficiency.

### Level Shifting (5V TTL ↔ 3.3V UART)

The MaCO2 outputs 5V TTL levels, which could damage the ESP32's 3.3V inputs.

**Recommended: Simple Resistor + Zener Circuit**

For MaCO2 TX → ESP32 RX:

```
MaCO2 TX (5V) ──┬─── 1kΩ ───┬──> ESP32 RX (3.3V max)
                │            │
                │          ┴ 3.3V Zener (1N5226B)
                │            │
                └────────────┴──> GND
```

**Component values:**
- **Resistor:** 1kΩ (current limiting)
- **Zener:** 3.3V, 500mW (1N5226B or BZX79-C3V3)
- Clamps voltage to safe 3.3V level
- Simple, reliable at 9600 baud

For ESP32 TX → MaCO2 RX:
- **Direct connection** - 3.3V is recognized as HIGH by 5V TTL (threshold ~2.0V)
- No level shifting needed in this direction

**Alternative: 74LVC245 (if you prefer IC solution)**

```
                    74LVC245 (DIP-20 or TSSOP-20)
                    VCC = 3.3V
                    
MaCO2 TX (5V) ───>│ A1      B1 ├───> ESP32 RX (3.3V)
                  │             │
ESP32 TX (3.3V) ──│ A2      B2 ├───> MaCO2 RX (direct works)
                  │             │
                  │ DIR = HIGH  │
                  │ OE = LOW    │
                  └─────────────┘
```

- 74LVC245 has 5V-tolerant inputs when powered by 3.3V
- Outputs are 3.3V, safe for ESP32
- More complex but handles multiple signals

### External ADC Connection

**Using ESP32-S3 Internal ADC**

The ESP32-S3 has two 12-bit SAR ADCs with multiple channels available on the T-Display S3:

```cpp
// Available ADC pins on T-Display S3
#define O2_SENSOR_PIN      1    // ADC1_CH0 - O2 sensor input
#define VOL_SENSOR_PIN     2    // ADC1_CH1 - Volume sensor input

// ADC Configuration
#define ADC_RESOLUTION     12   // 12-bit (0-4095)
#define ADC_ATTENUATION    ADC_11db  // 0-3.3V range (actually ~3.1V max)
```

**ADC Specifications:**
- **Resolution:** 12-bit (0-4095 counts)
- **Voltage Range:** 0-3.3V (with ADC_11db attenuation)
- **Accuracy:** ±2% typical
- **Sampling Rate:** Up to 83,000 samples/sec (more than adequate)
- **Channels Used:**
  - GPIO 1 (ADC1_CH0) → O2 sensor
  - GPIO 2 (ADC1_CH1) → Volume sensor

**Voltage Divider (if sensors output 5V):**

If your O2 or Volume sensors output 0-5V, add voltage dividers:

```
Sensor Output (0-5V) ──┬─── R1 (10kΩ) ───┬──> ESP32 ADC Pin (0-3.3V)
                       │                  │
                      GND               R2 (22kΩ)
                                          │
                                         GND

Divider ratio: 3.3V / 5V = 0.66
R2 / (R1 + R2) = 0.66
Use: R1=10kΩ, R2=22kΩ → Output = 3.44V (close enough)
Or: R1=10kΩ, R2=20kΩ → Output = 3.33V (ideal)
```

**Direct Connection (if sensors output 0-3.3V):**
- Connect sensor outputs directly to ESP32 ADC pins
- Add 100nF capacitor to ground near each ADC input for noise filtering

**ADC Calibration:**
The ESP32 ADC has built-in calibration support for better accuracy:
```cpp
#include <esp_adc_cal.h>

esp_adc_cal_characteristics_t adc_chars;
esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, 
                        ADC_WIDTH_BIT_12, 1100, &adc_chars);
```

### Complete System Block Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                     Power Supply                            │
│                                                             │
│  +12V Input ──┬──> MaCO2 Sensor                           │
│               │                                             │
│               └──> 7805 ──> +5V ──> ESP32 USB-C           │
│                    [Caps]                                   │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                  MaCO2 Sensor Interface                     │
│                                                             │
│  MaCO2 TX (5V) ──> 1kΩ + Zener ──> ESP32 RX (3.3V)        │
│                                      GPIO 44                │
│                                                             │
│  MaCO2 RX (5V TTL) <── Direct <── ESP32 TX (3.3V)         │
│                                      GPIO 43                │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                   Analog Sensors                            │
│                                                             │
│  O2 Sensor ──────> [Voltage Divider?] ──> ESP32 GPIO 1    │
│                                            (ADC1_CH0)       │
│                                                             │
│  Volume Sensor ──> [Voltage Divider?] ──> ESP32 GPIO 2    │
│                                            (ADC1_CH1)       │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                    ESP32 T-Display S3                       │
│  ┌────────────────┐                                        │
│  │  TFT Display   │  ◄─── Built-in SPI                    │
│  │  170x320 LCD   │                                        │
│  └────────────────┘                                        │
│                                                             │
│  WiFi ◄──> Web Server ◄──> Browser Clients                │
│                                                             │
│  USB-C ◄──┬──> PC (Programming/Debug)                     │
│           └──> LabVIEW (Serial/CDC)                        │
│                Data mirror & command interface              │
└─────────────────────────────────────────────────────────────┘
```

**Key Feature: USB-C as Universal Interface**
- **Programming:** Upload firmware via Arduino IDE / PlatformIO
- **Debugging:** Serial monitor output
- **LabVIEW Communication:** Virtual COM port (USB CDC)
- **Power:** Can be powered via USB-C (5V) OR external 7805
- Single cable connection to PC!

### Bill of Materials (BOM)

| Component | Part Number | Qty | Notes |
|-----------|-------------|-----|-------|
| Microcontroller | LilyGO T-Display S3 | 1 | With 1.9" display |
| Voltage Regulator | LM7805 (TO-220) | 1 | +12V to +5V |
| Level Shift Resistor | 1kΩ 1/4W | 1 | For UART RX protection |
| Zener Diode | 1N5226B (3.3V 500mW) | 1 | For UART RX protection |
| Voltage Divider R1 | 10kΩ 1/4W | 2 | For 5V sensors (if needed) |
| Voltage Divider R2 | 20kΩ 1/4W | 2 | For 5V sensors (if needed) |
| Ceramic Cap | 100nF | 6 | Power decoupling + ADC filtering |
| Electrolytic Cap | 10µF 25V | 2 | 7805 input/output filtering |
| Power Connector | 2-pin screw terminal | 1 | +12V input |
| TO-220 Heatsink | - | 1 | For 7805 (if needed) |
| Protoboard/PCB | - | 1 | Assembly base |

**Estimated cost:** ~$25-30 USD (excluding sensors)

**Notes:**
- Voltage dividers only needed if sensors output 5V
- If sensors are 0-3.3V, omit R1/R2 and connect directly
- Add 100nF cap to ground at each ADC input for noise filtering

## Data Flow Design

### Input Processing (from MaCO2 Sensor)

The ESP32 now receives data directly from the MaCO2 sensor (8 bytes):

```cpp
// MaCO2 sensor output format (from original PIC code analysis)
struct MaCO2Packet {
    uint8_t status1;        // d[0] - Status byte 1
    uint8_t status2;        // d[1] - Status byte 2  
    uint8_t rr;             // d[2] - Respiratory Rate
    uint8_t reserved1;      // d[3] - Unused/reserved
    uint8_t fetco2;         // d[4] - End-tidal CO2 (waveform value)
    uint8_t fico2;          // d[5] - Inspired CO2
    uint8_t reserved2;      // d[6] - Unused/reserved
    uint8_t reserved3;      // d[7] - Unused/reserved
};

// Enhanced data structure (after adding ADC readings)
struct CO2Data {
    // From MaCO2 sensor
    uint16_t co2_waveform;      // fetco2 (d[4]) - 0-255
    uint8_t status1;            // Data valid flag
    uint8_t status2;            // Pump, leak, occlusion bits
    uint8_t respiratory_rate;   // RR in breaths/min (d[2])
    uint8_t fico2;              // Inspired CO2 (d[5])
    uint8_t fetco2;             // End-tidal CO2 (d[4])
    
    // From external ADC (replaces PIC AN0/AN1)
    uint16_t o2_adc;            // O2 sensor reading (0-65535)
    uint16_t vol_adc;           // Volume sensor reading (0-1023)
    
    // Metadata
    uint32_t timestamp;         // millis()
};
```

### MaCO2 Initialization Sequence

According to the PIC firmware, the MaCO2 requires an initialization handshake:

```cpp
void initializeMaCO2() {
    // Wait for start byte (0x06) from MaCO2
    while (SerialMaCO2.read() != 0x06) {
        delay(10);
    }
    
    // Send acknowledgment (ESC = 0x1B)
    SerialMaCO2.write(0x1B);
    
    // Read and discard 7 initialization bytes
    for (int i = 0; i < 7; i++) {
        while (!SerialMaCO2.available());
        SerialMaCO2.read();
    }
    
    Serial.println("MaCO2 sensor initialized");
}
```

### Output Processing (to Host/LabVIEW)

For LabVIEW compatibility, the ESP32 formats data in the original PIC output format:

**Mode 1: TFT Display (Local)**
- Real-time CO2 waveform graph
- Numeric displays: EtCO2, FiCO2, RR
- O2 percentage
- Volume reading
- Status indicators (pump, leak, occlusion)

**Mode 2: Web Server (Remote)**
- HTML/JavaScript dashboard
- WebSocket for real-time updates
- Historical data charts (Chart.js)
- Data export (CSV download)
- Configuration interface

**Mode 3: Serial Passthrough (Legacy LabVIEW)**
- Mirror received data to another UART
- Maintains exact protocol for compatibility
- Bidirectional command passthrough

## Pin Assignment (T-Display S3)

```cpp
// Display (pre-configured for T-Display S3)
#define TFT_CS         5
#define TFT_DC         16
#define TFT_RST        23
#define TFT_MOSI       19
#define TFT_SCLK       18
#define TFT_BL         38    // Backlight

// MaCO2 Sensor Serial (UART1)
#define UART1_RX_MACO2  44   // RX from MaCO2 sensor (with level shift)
#define UART1_TX_MACO2  43   // TX to MaCO2 sensor (direct 3.3V)

// Onboard ADC Inputs (12-bit, 0-3.3V)
#define O2_SENSOR_PIN   1    // ADC1_CH0 - O2 sensor input
#define VOL_SENSOR_PIN  2    // ADC1_CH1 - Volume/Flow sensor input

// USB-C Port (built-in)
// Used for:
//  - Programming & debugging (Serial)
//  - LabVIEW communication (USB CDC Virtual COM)
//  - Power input (5V)

// User Interface
#define BUTTON_1        0    // Boot button
#define BUTTON_2        14   // Second button (if available)

// RGB LED (built-in, optional status indicator)
#define RGB_LED         48   // WS2812 RGB LED
```

**Pin Usage Summary:**
- **UART1 (GPIO 44/43):** MaCO2 sensor communication
- **USB-C (CDC):** Host/LabVIEW communication + power
- **ADC (GPIO 1/2):** O2 and Volume sensors
- **SPI (GPIO 5,16,19,18):** TFT display (pre-wired)
- **GPIO 0:** Boot button (user input)
- **GPIO 48:** RGB LED (status indicator)

## Software Architecture

### Core Components

#### 1. Serial Parser
```cpp
class MedAirParser {
private:
    enum State { WAIT_ESC, READ_CO2, READ_O2, READ_VOL, READ_STATUS };
    State state;
    String buffer;
    
public:
    bool parsePacket(Stream& serial, CO2Data& data);
    bool validateChecksum();
    void resetParser();
};
```

#### 2. Waveform Buffer
```cpp
class WaveformBuffer {
private:
    static const int BUFFER_SIZE = 512;  // ~5 seconds at 100Hz
    uint16_t co2_buffer[BUFFER_SIZE];
    int write_index;
    
public:
    void addSample(uint16_t co2);
    void getBuffer(uint16_t* dest, int count);
    uint16_t getMin();
    uint16_t getMax();
};
```

#### 3. TFT Display Manager
```cpp
class DisplayManager {
private:
    TFT_eSPI tft;
    WaveformBuffer& waveform;
    
public:
    void init();
    void drawWaveform();
    void updateNumericValues(CO2Data& data);
    void drawStatusIndicators(uint8_t status2);
    void drawAlarmBox(const char* message);
};
```

#### 4. Web Server
```cpp
class CO2WebServer {
private:
    AsyncWebServer server;
    AsyncWebSocket ws;
    
public:
    void init();
    void handleRoot();
    void handleDataAPI();
    void sendWebSocketUpdate(CO2Data& data);
    void handleConfigPage();
};
```

#### 5. Data Logger
```cpp
class DataLogger {
private:
    File dataFile;
    static const int LOG_INTERVAL_MS = 1000;
    
public:
    void init();
    void logData(CO2Data& data);
    String getCSVData(uint32_t start_time, uint32_t end_time);
};
```

## Implementation Phases

### Phase 1: Basic Serial Communication
- [ ] Setup UART communication with PIC
- [ ] Implement packet parser
- [ ] Parse all fields correctly
- [ ] Display raw values on Serial Monitor
- [ ] Implement zero-replacement decoding

### Phase 2: TFT Display
- [ ] Initialize TFT_eSPI library for T-Display S3
- [ ] Create waveform display area
- [ ] Display numeric values (EtCO2, FiCO2, RR, O2, VOL)
- [ ] Add status indicators (pump, leak, occlusion)
- [ ] Implement alarm notifications

### Phase 3: Web Server (Basic)
- [ ] Setup WiFi AP or Station mode
- [ ] Create basic HTML dashboard
- [ ] Implement RESTful API for current values
- [ ] Add WebSocket for real-time updates
- [ ] Display waveform using Chart.js

### Phase 4: Data Logging
- [ ] Setup SD card or SPIFFS storage
- [ ] Log data to CSV format
- [ ] Implement data retrieval API
- [ ] Add download functionality to web interface

### Phase 5: Legacy LabVIEW Support
- [ ] Implement serial passthrough on second UART
- [ ] Bidirectional command relay
- [ ] Test with original LabVIEW software

### Phase 6: Advanced Features
- [ ] Touch button configuration menu
- [ ] WiFi configuration via web interface
- [ ] Data export to external servers
- [ ] Alarm thresholds and notifications
- [ ] Battery monitoring (if powered)

## Code Structure

```
ESP32_MedAir/
├── src/
│   ├── main.cpp                 # Main Arduino sketch
│   ├── MedAirParser.h/cpp      # Serial protocol parser
│   ├── WaveformBuffer.h/cpp    # Circular buffer for waveform
│   ├── DisplayManager.h/cpp    # TFT display control
│   ├── WebServer.h/cpp         # HTTP/WebSocket server
│   ├── DataLogger.h/cpp        # SD/SPIFFS logging
│   └── Config.h                # Pin definitions, constants
├── data/                        # Web server files (SPIFFS)
│   ├── index.html
│   ├── style.css
│   ├── app.js
│   └── chart.min.js
├── test/
│   ├── test_parser.cpp         # Unit tests for parser
│   └── test_waveform.cpp       # Waveform buffer tests
└── platformio.ini              # PlatformIO configuration
```

## Display Layout (170x320 TFT)

```
┌─────────────────────────────┐
│ MedAir CO2 Monitor    WiFi● │  20px header
├─────────────────────────────┤
│                             │
│  ╱╲  ╱╲    ╱╲  ╱╲          │  
│ ╱  ╲╱  ╲  ╱  ╲╱  ╲         │  120px
│╱          ╱           ╲     │  waveform
│     CO2 Waveform            │
│                             │
├─────────────────────────────┤
│  EtCO2    FiCO2      RR     │  
│  38 mmHg   0 mmHg   16 bpm  │  60px
├─────────────────────────────┤  values
│  O2       Volume            │
│  21.0%    450 mL            │  40px
├─────────────────────────────┤
│ [●PUMP] [●OK] [○LEAK]      │  30px
│           Status            │  status
└─────────────────────────────┘
```

## Web Interface Design

### Dashboard Page (`index.html`)

```html
<!DOCTYPE html>
<html>
<head>
    <title>MedAir CO2 Monitor</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <script src="chart.min.js"></script>
</head>
<body>
    <div class="container">
        <h1>MedAir CO2 Monitor</h1>
        
        <!-- Real-time Values -->
        <div class="metrics">
            <div class="metric-box">
                <h3>EtCO2</h3>
                <p id="etco2" class="value">--</p>
                <span class="unit">mmHg</span>
            </div>
            <div class="metric-box">
                <h3>FiCO2</h3>
                <p id="fico2" class="value">--</p>
                <span class="unit">mmHg</span>
            </div>
            <div class="metric-box">
                <h3>RR</h3>
                <p id="rr" class="value">--</p>
                <span class="unit">bpm</span>
            </div>
            <div class="metric-box">
                <h3>O2</h3>
                <p id="o2" class="value">--</p>
                <span class="unit">%</span>
            </div>
        </div>
        
        <!-- Waveform Chart -->
        <canvas id="waveformChart"></canvas>
        
        <!-- Status Indicators -->
        <div class="status">
            <span id="pump-status" class="badge">Pump: OK</span>
            <span id="leak-status" class="badge">No Leak</span>
            <span id="occlusion-status" class="badge">No Occlusion</span>
        </div>
        
        <!-- Controls -->
        <div class="controls">
            <button onclick="startPump()">Start Pump</button>
            <button onclick="calibrateZero()">Zero Cal</button>
            <button onclick="downloadData()">Export CSV</button>
        </div>
    </div>
    
    <script src="app.js"></script>
</body>
</html>
```

### WebSocket Client (`app.js`)

```javascript
let ws;
let chart;
let waveformData = [];

function initWebSocket() {
    ws = new WebSocket(`ws://${window.location.hostname}/ws`);
    
    ws.onmessage = function(event) {
        const data = JSON.parse(event.data);
        updateDashboard(data);
    };
    
    ws.onerror = function(error) {
        console.error('WebSocket error:', error);
    };
}

function updateDashboard(data) {
    document.getElementById('etco2').textContent = data.fetco2;
    document.getElementById('fico2').textContent = data.fico2;
    document.getElementById('rr').textContent = data.rr;
    document.getElementById('o2').textContent = data.o2.toFixed(1);
    
    // Update waveform
    waveformData.push(data.co2_waveform);
    if (waveformData.length > 100) waveformData.shift();
    chart.update();
    
    // Update status indicators
    updateStatus(data.status2);
}

function updateStatus(status2) {
    const pumpOn = (status2 & 0x01) !== 0;
    const leak = (status2 & 0x02) !== 0;
    const occlusion = (status2 & 0x04) !== 0;
    
    document.getElementById('pump-status').textContent = 
        pumpOn ? 'Pump: Running' : 'Pump: Stopped';
    document.getElementById('leak-status').className = 
        leak ? 'badge error' : 'badge success';
    document.getElementById('occlusion-status').className = 
        occlusion ? 'badge error' : 'badge success';
}

function startPump() {
    fetch('/command?cmd=start_pump');
}

function calibrateZero() {
    fetch('/command?cmd=zero_cal');
}

function downloadData() {
    window.location.href = '/export?format=csv';
}

window.onload = function() {
    initWebSocket();
    initChart();
};
```

## Main ESP32 Code Structure

```cpp
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <esp_adc_cal.h>
#include "MedAirParser.h"
#include "DisplayManager.h"
#include "WebServer.h"

// Configuration
const char* ssid = "MedAir_Monitor";
const char* password = "co2monitor";

// Pin Definitions
#define UART1_RX_MACO2  44
#define UART1_TX_MACO2  43
#define O2_SENSOR_PIN   1    // ADC1_CH0
#define VOL_SENSOR_PIN  2    // ADC1_CH1

// Objects
HardwareSerial SerialMaCO2(1);    // UART1 for MaCO2 sensor (bidirectional)
// USB CDC (Serial) is used for LabVIEW communication automatically
MedAirParser parser;
DisplayManager display;
CO2WebServer webServer;
WaveformBuffer waveform;

CO2Data currentData;
bool enableLabViewMirror = true;

// ADC calibration
esp_adc_cal_characteristics_t adc_chars;

void setup() {
    // Initialize USB CDC for debugging and LabVIEW
    Serial.begin(115200);
    delay(1000);  // Wait for USB CDC to initialize
    Serial.println("MedAir CO2 Monitor Starting...");
    
    // Initialize MaCO2 sensor communication (bidirectional)
    SerialMaCO2.begin(9600, SERIAL_8N1, UART1_RX_MACO2, UART1_TX_MACO2);
    
    // Configure ADC
    analogReadResolution(12);  // 12-bit resolution (0-4095)
    analogSetAttenuation(ADC_11db);  // 0-3.3V range
    
    // Calibrate ADC for better accuracy
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, 
                            ADC_WIDTH_BIT_12, 1100, &adc_chars);
    
    pinMode(O2_SENSOR_PIN, INPUT);
    pinMode(VOL_SENSOR_PIN, INPUT);
    
    // Initialize display
    display.init();
    display.showSplash("MedAir CO2 Monitor", "Initializing...");
    
    // Initialize WiFi AP
    WiFi.softAP(ssid, password);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);
    
    // Initialize web server
    webServer.init();
    
    // Wait for MaCO2 sensor initialization
    initializeMaCO2();
    
    display.showSplash("Ready", IP.toString().c_str());
    delay(2000);
    
    Serial.println("System ready. Connect to WiFi or use USB for LabVIEW.");
}

void loop() {
    // Parse incoming data from MaCO2 sensor
    if (parser.parseMaCO2Packet(SerialMaCO2, currentData)) {
        
        // Sample onboard ADC channels
        uint16_t o2_raw = analogRead(O2_SENSOR_PIN);
        uint16_t vol_raw = analogRead(VOL_SENSOR_PIN);
        
        // Convert to calibrated voltage (mV)
        uint32_t o2_mv = esp_adc_cal_raw_to_voltage(o2_raw, &adc_chars);
        uint32_t vol_mv = esp_adc_cal_raw_to_voltage(vol_raw, &adc_chars);
        
        // Scale to match PIC ADC format for compatibility
        // PIC AN0: 0-65535 (16-bit, left-justified from 10-bit)
        // PIC AN1: 0-1023 (10-bit, right-justified)
        currentData.o2_adc = map(o2_raw, 0, 4095, 0, 65535);   // Scale to 16-bit
        currentData.vol_adc = map(vol_raw, 0, 4095, 0, 1023);  // Scale to 10-bit
        
        // Calculate O2 percentage (example - adjust for your sensor)
        // Common O2 sensors: 0-3.3V = 0-100% O2
        float o2_voltage = o2_mv / 1000.0;
        float o2_percent = (o2_voltage / 3.3) * 100.0;
        
        // Calculate volume (example - adjust for your sensor)
        float vol_voltage = vol_mv / 1000.0;
        float volume_ml = vol_voltage * 200.0;  // Example: 200mL per volt
        
        // Add to waveform buffer
        waveform.addSample(currentData.co2_waveform);
        
        // Update TFT display
        display.updateNumericValues(currentData, o2_percent, volume_ml);
        display.drawWaveform();
        
        // Send to web clients
        webServer.sendWebSocketUpdate(currentData);
        
        // Mirror enhanced data packet to LabVIEW via USB CDC
        if (enableLabViewMirror) {
            sendEnhancedPacket(Serial, currentData);
        }
        
        // Check for alarms
        checkAlarms(currentData);
    }
    
    // Handle web interface commands
    if (webServer.hasCommand()) {
        uint8_t cmd = webServer.getCommand();
        SerialMaCO2.write(cmd);  // Send directly to MaCO2 sensor
        Serial.printf("Web command to MaCO2: 0x%02X\n", cmd);
    }
    
    // Handle LabVIEW commands via USB CDC
    if (Serial.available()) {
        uint8_t cmd = Serial.read();
        // Check if it's a command (not debug text)
        if (cmd == 0xA5 || cmd == 0x5A) {
            SerialMaCO2.write(cmd);  // Forward to MaCO2 sensor
            Serial.printf("LabVIEW command forwarded: 0x%02X\n", cmd);
        }
    }
}

// Initialize MaCO2 sensor (from PIC firmware analysis)
void initializeMaCO2() {
    Serial.println("Waiting for MaCO2 sensor...");
    
    // Wait for start byte (0x06) from MaCO2
    unsigned long timeout = millis() + 10000;  // 10 second timeout
    while (millis() < timeout) {
        if (SerialMaCO2.available() && SerialMaCO2.read() == 0x06) {
            // Send acknowledgment (ESC = 0x1B)
            SerialMaCO2.write(0x1B);
            Serial.println("MaCO2 start byte received, sent ACK");
            
            // Read and discard 7 initialization bytes
            for (int i = 0; i < 7; i++) {
                while (!SerialMaCO2.available() && millis() < timeout);
                SerialMaCO2.read();
            }
            
            Serial.println("MaCO2 sensor initialized successfully");
            return;
        }
        delay(100);
    }
    
    Serial.println("WARNING: MaCO2 initialization timeout!");
}

// Send data in original PIC format for LabVIEW compatibility
void sendEnhancedPacket(Stream& stream, CO2Data& data) {
    // Format: <ESC>ABC<TAB>DEFGH<TAB>IJKLM<TAB>[Status1][Status2][RR][FiCO2][FetCO2]<CR><LF>
    char buffer[64];
    
    snprintf(buffer, sizeof(buffer), 
        "\x1B%03d\t%05d\t%05d\t%c%c%c%c%c\r\n",
        data.co2_waveform,
        data.o2_adc,
        data.vol_adc,
        data.status1,
        data.status2 == 0 ? 128 : data.status2,  // Zero replacement
        data.respiratory_rate == 0 ? 255 : data.respiratory_rate,
        data.fico2 == 0 ? 255 : data.fico2,
        data.fetco2 == 0 ? 255 : data.fetco2
    );
    
    stream.print(buffer);
}

void checkAlarms(CO2Data& data) {
    // Pump stopped
    if (!(data.status2 & 0x01)) {
        display.drawAlarmBox("PUMP STOPPED");
    }
    
    // Leak detected
    if (data.status2 & 0x02) {
        display.drawAlarmBox("LEAK DETECTED");
    }
    
    // Occlusion
    if (data.status2 & 0x04) {
        display.drawAlarmBox("OCCLUSION");
    }
    
    // Zero CO2 (no breathing detected)
    if (data.co2_waveform < 5 && data.fetco2 < 5) {
        display.drawAlarmBox("NO CO2 DETECTED");
    }
}
```

**USB CDC Notes:**
- `Serial` object is the USB CDC port (Virtual COM port)
- Appears as COM port on Windows, /dev/ttyACM0 on Linux
- LabVIEW can open this port just like any serial device
- Same port used for Arduino IDE Serial Monitor
- Baud rate doesn't matter for USB CDC (it's USB, not real UART)
- Set LabVIEW to 115200 baud by convention

## Required Libraries

```ini
# platformio.ini
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
    -DUSER_SETUP_LOADED=1
    -DST7789_DRIVER=1
    -DTFT_WIDTH=170
    -DTFT_HEIGHT=320
    -DTFT_MOSI=19
    -DTFT_SCLK=18
    -DTFT_CS=5
    -DTFT_DC=16
    -DTFT_RST=23
```

## Testing Strategy

### 1. Serial Parser Test
```cpp
// Send test packet via Serial Monitor
// <ESC>038<TAB>21000<TAB>00450<TAB> [status1][status2][16][0][38]<CR><LF>
```

### 2. Display Test
```cpp
// Inject fake data to verify display rendering
CO2Data testData = {
    .co2_waveform = 38,
    .o2_adc = 21000,
    .vol_adc = 450,
    .status1 = 0xFF,
    .status2 = 0x01,  // Pump running
    .respiratory_rate = 16,
    .fico2 = 0,
    .fetco2 = 38
};
```

### 3. WebSocket Test
```javascript
// Browser console test
const ws = new WebSocket('ws://192.168.4.1/ws');
ws.onmessage = (e) => console.log(JSON.parse(e.data));
```

## Configuration Options

Store in EEPROM/Preferences:
- WiFi SSID and password
- Alarm thresholds (EtCO2 low/high, RR low/high)
- Display brightness
- Data logging interval
- Waveform sweep speed
- Unit preferences (mmHg vs kPa)

## Power Considerations

- USB-C powered (5V)
- Typical consumption: ~200-300mA
- Display backlight: ~100mA
- WiFi active: ~80mA
- Optional: Add battery for portability (18650 Li-ion)

## Future Enhancements

1. **MQTT Support**: Push data to hospital monitoring systems
2. **Multi-device**: Monitor multiple patients simultaneously
3. **Cloud Logging**: Upload to database for long-term analysis
4. **Mobile App**: Native iOS/Android companion app
5. **Bluetooth**: BLE for direct phone connection
6. **Trend Analysis**: Automatic detection of respiratory patterns

## Backward Compatibility Notes

**For Dr. Eaglehagen's LabVIEW:**

The ESP32 replaces the PIC entirely and uses USB CDC for LabVIEW communication.

### USB CDC Virtual COM Port

The ESP32's USB-C port creates a **virtual COM port** (USB CDC) that LabVIEW can access directly:

```
ESP32 USB-C ──> PC USB Port ──> Virtual COM Port ──> LabVIEW VISA
```

**LabVIEW Configuration:**
1. Connect ESP32 via USB-C cable to PC
2. Windows: Device appears as "USB Serial Device (COMx)"
3. Linux: Appears as /dev/ttyACM0
4. LabVIEW: Open COM port at 115200 baud (convention, actual baud doesn't matter for USB)
5. Data format: Identical to original PIC output

**Advantages over old system:**
- Single cable (power + data)
- No separate USB-Serial adapter needed
- Same data format as PIC
- Can simultaneously:
  - Send data to LabVIEW
  - Receive commands from LabVIEW
  - Provide debug output
  - Serve web interface via WiFi

### Migration Options

#### Option 1: USB-Only Operation (Simplest)
```
MaCO2 ←→ ESP32 USB-C ←→ PC/LabVIEW
         WiFi ←→ Browser (optional)
```
- Single USB-C cable to PC
- LabVIEW gets data via USB CDC
- Web interface available via WiFi simultaneously
- No additional hardware needed

#### Option 2: WiFi-Only Operation
```
MaCO2 ←→ ESP32 WiFi ←→ Browser
         (no PC needed)
```
- Standalone operation
- Access via any WiFi device
- No LabVIEW dependency
- Perfect for bedside monitoring

#### Option 3: Dual Mode
```
MaCO2 ←→ ESP32 ──┬──> USB-C ──> PC/LabVIEW
                 └──> WiFi ──> Multiple Browsers
```
- Use both interfaces simultaneously
- LabVIEW for data logging/analysis
- Web interface for quick checks
- Maximum flexibility

### Hardware Migration

**What Changes:**
- Remove PIC16F876A board
- Add ESP32 T-Display S3
- Connect O2/Volume sensors directly to ESP32 ADC (with voltage dividers if 5V)
- Single USB-C cable to PC (replaces old serial cable)

**What Stays the Same:**
- MaCO2 sensor (same serial connection, 9600 baud)
- O2 sensor (may need voltage divider)
- Volume/Flow sensor (may need voltage divider)
- Data packet format (identical to PIC output)
- LabVIEW code (zero changes required!)

**Wiring Comparison:**

*Old System:*
```
MaCO2 TX ──→ PIC RX
PIC TX ──→ RS232 converter ──→ PC COM port
PC ──→ MaCO2 RX (direct)
O2 Sensor ──→ PIC AN0
Volume Sensor ──→ PIC AN1
```

*New System:*
```
MaCO2 TX ──→ ESP32 UART1 RX (with level shift)
MaCO2 RX ──→ ESP32 UART1 TX (direct)
O2 Sensor ──→ ESP32 ADC GPIO1 (with divider if 5V)
Volume Sensor ──→ ESP32 ADC GPIO2 (with divider if 5V)
ESP32 USB-C ──→ PC (Virtual COM port for LabVIEW)
```

### LabVIEW Integration Details

**VISA Configuration:**
```
Port: COMx (Windows) or /dev/ttyACM0 (Linux)
Baud Rate: 115200 (convention)
Data Bits: 8
Parity: None
Stop Bits: 1
Flow Control: None
```

**Data Format (Unchanged):**
```
<ESC>ABC<TAB>DEFGH<TAB>IJKLM<TAB>[Status1][Status2][RR][FiCO2][FetCO2]<CR><LF>
```

**Command Format (Unchanged):**
- 0xA5: Start pump
- 0x5A: Zero calibration

**No LabVIEW Code Changes Required!**
- Same data parsing
- Same command sending
- Only difference: Open different COM port number
- Everything else identical

### Transition Strategy

1. **Phase 1: Parallel Testing**
   - Keep old PIC system operational
   - Build ESP32 system
   - Test with LabVIEW using USB CDC
   - Verify data matches PIC output

2. **Phase 2: Soft Migration**
   - Switch to ESP32 for normal monitoring
   - Keep PIC system as backup
   - Use web interface alongside LabVIEW

3. **Phase 3: Full Migration**
   - Remove PIC system
   - Use ESP32 exclusively
   - Retire old hardware

4. **Phase 4: Web-First** (Optional)
   - Transition away from LabVIEW
   - Use web interface as primary
   - Export data as needed

## Getting Started

1. Flash ESP32 with firmware
2. Connect to "MedAir_Monitor" WiFi network (password: co2monitor)
3. Open browser to http://192.168.4.1
4. Monitor data in real-time
5. For LabVIEW: Connect USB-Serial to UART2 pins

---

**Project Status:** Design Phase  
**Target Completion:** [Your timeline]  
**Contact:** [Your info for Dr. Eaglehagen]
