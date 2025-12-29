# ESP32 MedAir CO2 Monitor - Library Dependencies

## Required Libraries

### 1. TFT_eSPI
**Purpose:** TFT display driver for LilyGO T-Display S3  
**Used by:** DisplayManager  
**GitHub:** https://github.com/Bodmer/TFT_eSPI  
**Version:** 2.5.0 or newer  

**Installation:**
- **Arduino IDE:** Library Manager → Search "TFT_eSPI" → Install
- **PlatformIO Library Manager:** 
  - Search: `TFT_eSPI`
  - Author: Bodmer
  - Click "Add to Project"
- **PlatformIO platformio.ini:** Add to lib_deps:
  ```ini
  lib_deps = bodmer/TFT_eSPI@^2.5.0
  ```

**Configuration Required:** YES - Must configure for T-Display S3
- Navigate to library folder: `Arduino/libraries/TFT_eSPI/`
- Edit `User_Setup_Select.h`
- Comment out default, enable T-Display S3 setup OR
- Create custom `User_Setup.h` with these settings:

```cpp
// User_Setup.h for LilyGO T-Display S3
#define ST7789_DRIVER
#define TFT_WIDTH  170
#define TFT_HEIGHT 320

#define TFT_MOSI 19
#define TFT_SCLK 18
#define TFT_CS   5
#define TFT_DC   16
#define TFT_RST  23
#define TFT_BL   38

#define LOAD_GLCD   // Font 1
#define LOAD_FONT2  // Font 2
#define LOAD_FONT4  // Font 4
#define LOAD_FONT6  // Font 6
#define LOAD_FONT7  // Font 7
#define LOAD_FONT8  // Font 8
#define LOAD_GFXFF  // FreeFonts

#define SMOOTH_FONT

#define SPI_FREQUENCY  40000000
#define SPI_READ_FREQUENCY  20000000
```

---

### 2. ESPAsyncWebServer
**Purpose:** Asynchronous web server for WiFi interface  
**Used by:** WiFiManager  
**GitHub:** https://github.com/me-no-dev/ESPAsyncWebServer  
**Version:** Latest from GitHub (no official releases)  

**Installation:**
- **⚠️ NOT available in PlatformIO Library Manager** (not in registry)
- **Arduino IDE:** 
  1. Download ZIP from GitHub: https://github.com/me-no-dev/ESPAsyncWebServer
  2. Sketch → Include Library → Add .ZIP Library
- **PlatformIO platformio.ini:** Add GitHub URL to lib_deps:
  ```ini
  lib_deps = 
      https://github.com/me-no-dev/ESPAsyncWebServer.git
  ```
  PlatformIO will automatically clone from GitHub during build.

**Features:**
- Non-blocking web server
- WebSocket support
- Multiple concurrent clients
- Low memory footprint

---

### 3. AsyncTCP
**Purpose:** Asynchronous TCP library (required by ESPAsyncWebServer)  
**Used by:** WiFiManager (indirectly)  
**GitHub:** https://github.com/me-no-dev/AsyncTCP  
**Version:** Latest from GitHub  

**Installation:**
- **⚠️ NOT available in PlatformIO Library Manager** (not in registry)
- **Arduino IDE:** 
  1. Download ZIP from GitHub: https://github.com/me-no-dev/AsyncTCP
  2. Sketch → Include Library → Add .ZIP Library
- **PlatformIO platformio.ini:** Add GitHub URL to lib_deps:
  ```ini
  lib_deps = 
      https://github.com/me-no-dev/AsyncTCP.git
  ```
  PlatformIO will automatically clone from GitHub during build.

**Note:** This is a dependency of ESPAsyncWebServer, not directly included in code.

---

### 4. ArduinoJson
**Purpose:** JSON serialization/deserialization for web API  
**Used by:** WiFiManager  
**GitHub:** https://github.com/bblanchon/ArduinoJson  
**Documentation:** https://arduinojson.org/  
**Version:** 6.x or 7.x (v6 recommended)  

**Installation:**
- **Arduino IDE:** Library Manager → Search "ArduinoJson" → Install
- **PlatformIO Library Manager:**
  - Search: `ArduinoJson`
  - Author: Benoit Blanchon
  - Click "Add to Project"
  - **Use version 6.x** (not 5.x or 7.x)
- **PlatformIO platformio.ini:** Add to lib_deps:
  ```ini
  lib_deps = bblanchon/ArduinoJson@^6.21.0
  ```

**Used for:**
- Converting CO2Data to JSON for web interface
- Parsing commands from web clients
- WebSocket message formatting

---

## Built-in ESP32 Libraries (No Installation Required)

### 5. WiFi.h
**Purpose:** ESP32 WiFi functionality  
**Included with:** ESP32 Arduino Core  
**Used by:** WiFiManager  
**Documentation:** https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html

**Features:**
- Station mode (connect to existing WiFi)
- Access Point mode (create hotspot)
- WiFi events and status

---

### 6. esp_adc_cal.h
**Purpose:** ESP32 ADC calibration for accurate voltage readings  
**Included with:** ESP32 Arduino Core  
**Used by:** ADCManager  
**Documentation:** https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc.html

**Features:**
- Calibration curve for ADC
- Compensates for ESP32 ADC non-linearity
- More accurate voltage readings

---

### 7. Arduino.h
**Purpose:** Core Arduino functionality  
**Included with:** Arduino Core  
**Used by:** All modules  

Standard functions:
- `millis()`, `delay()`
- `Serial`, `pinMode()`, `digitalRead()`, `digitalWrite()`
- `analogRead()`, `analogWrite()`
- String handling

---

### 8. HardwareSerial
**Purpose:** Hardware UART communication  
**Included with:** ESP32 Arduino Core  
**Used by:** MaCO2Parser, main.cpp  

**Features:**
- Multiple UART ports (UART0, UART1, UART2)
- Configurable baud rates
- Hardware flow control support

---

## PlatformIO Library Manager Quick Reference

### Libraries Available in PlatformIO Registry (Search & Add):

| Library | Search Term | Author | PlatformIO ID |
|---------|-------------|--------|---------------|
| TFT_eSPI | `TFT_eSPI` | Bodmer | `bodmer/TFT_eSPI` |
| ArduinoJson | `ArduinoJson` | Benoit Blanchon | `bblanchon/ArduinoJson` |

**How to install:**
1. Open PlatformIO Home (house icon in left sidebar)
2. Click "Libraries" tab
3. Search for library name
4. Click on result
5. Click "Add to Project"
6. Select your environment
7. PlatformIO updates platformio.ini automatically

### Libraries NOT in Registry (Use GitHub URL):

| Library | Must Use GitHub URL |
|---------|---------------------|
| ESPAsyncWebServer | `https://github.com/me-no-dev/ESPAsyncWebServer.git` |
| AsyncTCP | `https://github.com/me-no-dev/AsyncTCP.git` |

**How to install:**
- Manually add GitHub URLs to `lib_deps` in platformio.ini
- PlatformIO will clone automatically on first build
- No need to download manually

---

## Complete PlatformIO Configuration

Create `platformio.ini` in your project root:

```ini
[env:lilygo-t-display-s3]
platform = espressif32
board = lilygo-t-display-s3
framework = arduino

# Set upload speed
upload_speed = 921600
monitor_speed = 115200

# Board-specific settings
board_build.flash_mode = qio
board_build.partitions = default.csv

# Library dependencies
lib_deps = 
    bodmer/TFT_eSPI@^2.5.0
    https://github.com/me-no-dev/ESPAsyncWebServer.git
    https://github.com/me-no-dev/AsyncTCP.git
    bblanchon/ArduinoJson@^6.21.0

# Build flags for TFT_eSPI configuration
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
    -DTFT_BL=38
    -DLOAD_GLCD=1
    -DLOAD_FONT2=1
    -DLOAD_FONT4=1
    -DLOAD_FONT6=1
    -DLOAD_FONT7=1
    -DLOAD_FONT8=1
    -DLOAD_GFXFF=1
    -DSMOOTH_FONT=1
    -DSPI_FREQUENCY=40000000
    -DSPI_READ_FREQUENCY=20000000
```

---

## Step-by-Step: Installing Libraries in PlatformIO

### Method 1: Using PlatformIO Library Manager (GUI)

**For TFT_eSPI and ArduinoJson:**

1. **Open PlatformIO Home**
   - Click the house icon (🏠) in the left sidebar
   - Or: PlatformIO menu → Home

2. **Open Libraries Tab**
   - Click "Libraries" in the left menu

3. **Search for TFT_eSPI**
   - Type `TFT_eSPI` in search box
   - Find result by "Bodmer"
   - Click on it

4. **Add to Project**
   - Click "Add to Project" button
   - Select your project/environment
   - PlatformIO automatically updates platformio.ini

5. **Repeat for ArduinoJson**
   - Search: `ArduinoJson`
   - Author: Benoit Blanchon
   - Add to project

### Method 2: Editing platformio.ini (Recommended)

**For ALL libraries including GitHub ones:**

1. **Open platformio.ini** in your project root

2. **Add lib_deps section** under your environment:
   ```ini
   [env:lilygo-t-display-s3]
   platform = espressif32
   board = lilygo-t-display-s3
   framework = arduino
   
   lib_deps = 
       bodmer/TFT_eSPI@^2.5.0
       bblanchon/ArduinoJson@^6.21.0
       https://github.com/me-no-dev/ESPAsyncWebServer.git
       https://github.com/me-no-dev/AsyncTCP.git
   ```

3. **Save the file**

4. **Build your project**
   - PlatformIO automatically downloads all libraries
   - First build takes longer (downloading)
   - Subsequent builds use cached libraries

**That's it!** PlatformIO handles everything automatically.

### Method 3: Command Line (PlatformIO Core)

If you prefer terminal:

```bash
# Navigate to project directory
cd your_project_folder

# Install from PlatformIO registry
pio lib install "bodmer/TFT_eSPI@^2.5.0"
pio lib install "bblanchon/ArduinoJson@^6.21.0"

# For GitHub libraries, just add to platformio.ini
# (No command line install for GitHub URLs)
```

---

## Arduino IDE Setup

### 1. Install ESP32 Board Support
1. File → Preferences
2. Additional Board Manager URLs:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Tools → Board → Boards Manager
4. Search "ESP32" → Install "esp32 by Espressif Systems"
5. Tools → Board → ESP32 Arduino → "ESP32S3 Dev Module" or find "LilyGO T-Display S3"

### 2. Board Settings (Tools Menu)
```
Board: "ESP32S3 Dev Module" or "LilyGO T-Display S3"
USB CDC On Boot: "Enabled"
CPU Frequency: "240MHz (WiFi)"
Flash Mode: "QIO 80MHz"
Flash Size: "16MB (128Mb)"
Partition Scheme: "Default 4MB with spiffs"
PSRAM: "OPI PSRAM"
Upload Mode: "UART0 / Hardware CDC"
Upload Speed: "921600"
USB Mode: "Hardware CDC and JTAG"
```

### 3. Install Libraries
Tools → Manage Libraries, then search and install:
- TFT_eSPI (by Bodmer)
- ArduinoJson (by Benoit Blanchon)

For ESPAsyncWebServer and AsyncTCP:
1. Download as ZIP from GitHub
2. Sketch → Include Library → Add .ZIP Library
3. Select downloaded files

---

## Library Sizes (Approximate)

| Library | Flash Usage | RAM Usage | Notes |
|---------|-------------|-----------|-------|
| TFT_eSPI | ~50KB | ~10KB | Display driver |
| ESPAsyncWebServer | ~60KB | ~20KB | Web server |
| AsyncTCP | ~20KB | ~5KB | TCP/IP stack |
| ArduinoJson | ~15KB | ~2KB | JSON parser |
| **Total Libraries** | **~145KB** | **~37KB** | |
| **Your Code** | ~30-50KB | ~10-20KB | Estimate |
| **ESP32 Available** | 16MB Flash | 512KB RAM | Plenty of room! |

---

## Troubleshooting Library Issues

### TFT_eSPI Display Not Working
**Problem:** Screen stays white/black  
**Solution:** 
1. Check `User_Setup.h` configuration
2. Verify pin definitions match T-Display S3
3. Test with TFT_eSPI example: `File → Examples → TFT_eSPI → 160x128 → TFT_graphicstest_small`

### ESPAsyncWebServer Compile Errors
**Problem:** `'AsyncWebServer' does not name a type`  
**Solution:**
1. Ensure AsyncTCP is installed (required dependency)
2. Check ESP32 board support is up to date (v2.0.0+)
3. Verify correct board selected in Tools menu

### ArduinoJson Errors
**Problem:** Version conflicts  
**Solution:**
- Use version 6.x (not 5.x or 7.x beta)
- Check: `#include <ArduinoJson.h>` not `<ArduinoJSON.h>`
- Read migration guide if upgrading: https://arduinojson.org/v6/doc/upgrade/

### esp_adc_cal.h Not Found
**Problem:** `fatal error: esp_adc_cal.h: No such file or directory`  
**Solution:**
- Update ESP32 core to v2.0.0 or later
- For older cores, use `#include "driver/adc.h"` instead
- Or remove ADC calibration and use basic `analogRead()`

---

## Minimal Version (If Issues with Libraries)

If you have trouble with ESPAsyncWebServer, you can create a minimal version:

```cpp
// Remove WiFiManager completely
// Remove #include "WiFiManager.h"
// Comment out WiFi initialization in main.cpp
// Focus on: MaCO2Parser + ADCManager + DisplayManager + DataLogger
```

This gives you:
- ✅ TFT display with waveforms
- ✅ LabVIEW USB CDC output  
- ✅ MaCO2 sensor communication
- ✅ ADC readings
- ❌ No web interface (can add later)

---

## Alternative Libraries (If Needed)

### Instead of ESPAsyncWebServer:
- **ESP32 WebServer** (built-in, blocking but simpler)
  ```cpp
  #include <WebServer.h>
  WebServer server(80);
  ```

### Instead of ArduinoJson:
- Manual string formatting (not recommended)
- Or use Arduino's built-in String class

### Instead of TFT_eSPI:
- **Adafruit_ST7789** (slower but simpler)
- **LovyanGFX** (faster, more features)

---

## Quick Start Checklist

- [ ] Install ESP32 board support (v2.0.0+)
- [ ] Install TFT_eSPI library
- [ ] Configure TFT_eSPI User_Setup.h for T-Display S3
- [ ] Install ESPAsyncWebServer (ZIP from GitHub)
- [ ] Install AsyncTCP (ZIP from GitHub)  
- [ ] Install ArduinoJson (v6.x from Library Manager)
- [ ] Select correct board: ESP32S3 Dev / LilyGO T-Display S3
- [ ] Set USB CDC On Boot: Enabled
- [ ] Test compile with a simple example first
- [ ] Then compile the full MedAir project

---

## Support Resources

- **TFT_eSPI:** https://github.com/Bodmer/TFT_eSPI/discussions
- **ESPAsyncWebServer:** https://github.com/me-no-dev/ESPAsyncWebServer/issues
- **ESP32 Arduino:** https://github.com/espressif/arduino-esp32
- **LilyGO T-Display S3:** https://github.com/Xinyuan-LilyGO/T-Display-S3
- **ArduinoJson:** https://arduinojson.org/
- **ESP32 Forum:** https://www.esp32.com/

---

## License Notes

All listed libraries are open source:
- TFT_eSPI: FreeBSD License
- ESPAsyncWebServer: LGPL 2.1
- AsyncTCP: LGPL 2.1  
- ArduinoJson: MIT License
- ESP32 Core: Apache 2.0

Your project can use any compatible license.
