// main.cpp
// Main application for ESP32 MedAir CO2 Monitor
// Coordinates all subsystems and manages data flow

#include <Arduino.h>
#include "MaCO2Parser.h"
#include "ADCManager.h"
#include "DisplayManager.h"
#include "WiFiManager.h"
#include "DataLogger.h"

// ============================================================================
// Configuration
// ============================================================================

// WiFi Settings
const char* WIFI_SSID = "EAGLEHAGEN";
const char* WIFI_PASSWORD = "co2monitor";
const bool WIFI_AP_MODE = true;  // true = Access Point, false = Station

// Pin Definitions
#define UART_RX_MACO2   44
#define UART_TX_MACO2   43
#define O2_SENSOR_PIN   1
#define VOL_SENSOR_PIN  2

// Update intervals
#define DATA_UPDATE_INTERVAL_MS     125   // 8Hz data acquisition
#define DISPLAY_UPDATE_INTERVAL_MS  50    // 20Hz display refresh
#define WIFI_UPDATE_INTERVAL_MS     125   // 8Hz web update (match data rate)
#define LABVIEW_UPDATE_INTERVAL_MS  125   // 8Hz LabVIEW output

// ============================================================================
// Global Objects
// ============================================================================

HardwareSerial SerialMaCO2(1);  // UART1 for MaCO2 sensor

MaCO2Parser maco2Parser;
ADCManager adcManager(O2_SENSOR_PIN, VOL_SENSOR_PIN);
DisplayManager displayManager;
WiFiManager wifiManager;
DataLogger dataLogger;

CO2Data currentData;

// Timing variables
unsigned long lastDataUpdate = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastWiFiUpdate = 0;
unsigned long lastLabViewUpdate = 0;

// ============================================================================
// Setup
// ============================================================================

void setup() {
    // Initialize USB CDC serial for debugging and LabVIEW
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=================================");
    Serial.println("MedAir CO2 Monitor - ESP32");
    Serial.println("=================================\n");
    
    // Initialize Display
    Serial.println("Initializing display...");
    if (!displayManager.begin()) {
        Serial.println("ERROR: Display initialization failed!");
        while(1) delay(1000);
    }
    displayManager.showSplash("Teknosofen", "Initializing...");
    delay(2000);
    
    // Initialize ADC Manager
    Serial.println("Initializing ADC...");
    if (!adcManager.begin()) {
        Serial.println("ERROR: ADC initialization failed!");
        while(1) delay(1000);
    }
    
    // Set ADC calibration (adjust these for your sensors)
    // O2 sensor: 0V = 0%, 3.3V = 100% (linear, example values)
    adcManager.setO2Calibration(0.0, 3.3);
    
    // Volume sensor: 200 mL per volt (example value)
    adcManager.setVolumeCalibration(200.0, 0.0);
    
    // Initialize MaCO2 communication
    Serial.println("Initializing MaCO2 communication...");
    displayManager.showSplash("Teknosofen", "Connecting sensor...");
    SerialMaCO2.begin(9600, SERIAL_8N1, UART_RX_MACO2, UART_TX_MACO2);
    
    if (!maco2Parser.initialize(SerialMaCO2, 10000)) {
        Serial.println("WARNING: MaCO2 initialization timeout");
        Serial.println("Continuing anyway - sensor may connect later");
        delay(1000);
    }
    
    // Initialize WiFi
    Serial.println("Initializing WiFi...");
    if (WIFI_AP_MODE) {
        if (wifiManager.beginAP(WIFI_SSID, WIFI_PASSWORD)) {
            Serial.printf("AP Mode: SSID='%s', IP=%s\n", 
                         WIFI_SSID, wifiManager.getIP().toString().c_str());
        } else {
            Serial.println("WARNING: WiFi AP failed to start");
        }
    }
    
    // Start web server
    if (wifiManager.startServer()) {
        Serial.println("Web server started");
    } else {
        Serial.println("WARNING: Web server failed to start");
    }
    
    // Initialize Data Logger
    dataLogger.begin();
    dataLogger.setLabViewEnabled(true);  // Enable LabVIEW output via USB CDC
    
    // Show ready screen with IP
    char ipStr[32];
    snprintf(ipStr, sizeof(ipStr), "IP: %s", wifiManager.getIP().toString().c_str());
    displayManager.showSplash("Ready!", ipStr);
    delay(3000);
    
    // Clear screen and set network info for status display
    displayManager.clearScreen();
    displayManager.setNetworkInfo(WIFI_SSID, wifiManager.getIP().toString().c_str());
    
    Serial.println("\n=== System Ready ===");
    Serial.println("USB CDC: LabVIEW data output enabled");
    Serial.printf("WiFi: Connect to '%s' and open http://%s\n", 
                  WIFI_SSID, wifiManager.getIP().toString().c_str());
    Serial.println("====================\n");
    
    // Initialize current data structure
    memset(&currentData, 0, sizeof(currentData));
}

// ============================================================================
// Main Loop
// ============================================================================

void loop() {
    unsigned long now = millis();
    
    // -------------------------------------------------------------------------
    // Data Acquisition (10Hz)
    // -------------------------------------------------------------------------
    if (now - lastDataUpdate >= DATA_UPDATE_INTERVAL_MS) {
        lastDataUpdate = now;
        
        // Parse MaCO2 data (non-blocking)
        if (maco2Parser.parsePacket(SerialMaCO2, currentData)) {
            // Got new data from sensor
            
            // Update ADC readings
            adcManager.update(currentData);
            
            // Add waveform point to display buffer
            displayManager.addWaveformPoint(currentData.co2_waveform);
        }
    }
    
    // -------------------------------------------------------------------------
    // Display Update (20Hz)
    // -------------------------------------------------------------------------
    if (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL_MS) {
        lastDisplayUpdate = now;
        displayManager.updateAll(currentData);
    }
    
    // -------------------------------------------------------------------------
    // WiFi/Web Update (2Hz)
    // -------------------------------------------------------------------------
    if (now - lastWiFiUpdate >= WIFI_UPDATE_INTERVAL_MS) {
        lastWiFiUpdate = now;
        wifiManager.update(currentData);
    }
    
    // -------------------------------------------------------------------------
    // LabVIEW Output (10Hz)
    // -------------------------------------------------------------------------
    if (now - lastLabViewUpdate >= LABVIEW_UPDATE_INTERVAL_MS) {
        lastLabViewUpdate = now;
        
        if (currentData.valid) {
            dataLogger.sendPICFormat(Serial, currentData);
        }
    }
    
    // -------------------------------------------------------------------------
    // Handle Commands
    // -------------------------------------------------------------------------
    
    // Commands from web interface
    if (wifiManager.hasCommand()) {
        uint8_t cmd = wifiManager.getCommand();
        maco2Parser.sendCommand(SerialMaCO2, (MaCO2Command)cmd);
    }
    
    // Commands from LabVIEW via USB CDC
    if (Serial.available()) {
        uint8_t cmd = Serial.read();
        if (cmd == CMD_START_PUMP || cmd == CMD_ZERO_CAL) {
            maco2Parser.sendCommand(SerialMaCO2, (MaCO2Command)cmd);
        }
    }
    
    // WiFi manager loop (handles WebSocket events)
    wifiManager.loop();
    
    // Small delay to prevent watchdog issues
    delay(1);
}

// ============================================================================
// Helper Functions
// ============================================================================

void printStatus() {
    Serial.println("\n=== System Status ===");
    Serial.printf("MaCO2 Packets: %lu (errors: %lu)\n", 
                  maco2Parser.getPacketCount(), 
                  maco2Parser.getErrorCount());
    Serial.printf("LabVIEW Packets: %lu (%lu bytes)\n",
                  dataLogger.getPacketsSent(),
                  dataLogger.getBytesSent());
    Serial.printf("WiFi Clients: %d\n", wifiManager.getClientCount());
    Serial.printf("O2: %.1f%% (raw: %d, %.3fV)\n",
                  currentData.o2_percent,
                  adcManager.getO2Raw(),
                  adcManager.getO2Voltage());
    Serial.printf("Vol: %.1f mL (raw: %d, %.3fV)\n",
                  currentData.volume_ml,
                  adcManager.getVolRaw(),
                  adcManager.getVolVoltage());
    Serial.printf("CO2: FetCO2=%d, FCO2=%d, RR=%d\n",
                  currentData.fetco2,
                  currentData.fco2,
                  currentData.respiratory_rate);
    Serial.printf("Status: Pump=%s, Leak=%s, Occlusion=%s\n",
                  maco2Parser.isPumpRunning(currentData) ? "ON" : "OFF",
                  maco2Parser.isLeakDetected(currentData) ? "YES" : "NO",
                  maco2Parser.isOcclusionDetected(currentData) ? "YES" : "NO");
    Serial.println("====================\n");
}
