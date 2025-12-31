// DisplayManager.h
// Manages TFT display for LilyGO T-Display S3
// Handles waveform plotting, numeric displays, and status indicators

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include <TFT_eSPI.h>
#include "MaCO2Parser.h"  // For CO2Data structure

// Custom soft color palette
#define TFT_LOGOBACKGROUND       0x85BA
#define TFT_LOGOBLUE             0x5497
#define TFT_DARKERBLUE           0x3A97  // Muted steel blue
#define TFT_DEEPBLUE             0x1A6F  // Darker steel blue
#define TFT_SLATEBLUE            0x2B4F  // Lighter steel blue
#define TFT_MIDNIGHTBLUE         0x1028  // Light steel blue
#define TFT_REDDISH_TINT         0xA4B2
#define TFT_GREENISH_TINT        0x5DAD
#define TFT_STRONGER_GREEN       0x07E0  // Stronger green for WiFi indicator

class DisplayManager {
public:
    DisplayManager();
    
    // Initialize display
    bool begin();
    
    // Display splash screen
    void showSplash(const char* title, const char* subtitle = nullptr);
    
    // Clear entire screen
    void clearScreen();
    
    // Set network info to display
    void setNetworkInfo(const char* ssid, const char* ip);
    
    // Update all display elements
    void updateAll(const CO2Data& data);
    
    // Update individual sections
    void updateWaveform(const CO2Data& data);
    void updateNumericValues(const CO2Data& data);
    void updateStatusIndicators(const CO2Data& data);
    
    // Backlight control
    void setBacklight(uint8_t brightness);  // 0-255
    void backlightOn();
    void backlightOff();
    
    // Display settings
    void setWaveformSpeed(uint8_t speed);  // Pixels per update (1-10)
    void setRefreshRate(uint16_t rate_ms); // Minimum time between updates
    
    // Add data point to waveform buffer
    void addWaveformPoint(uint16_t co2_value);
    
private:
    TFT_eSPI _tft;
    
    // Display dimensions (T-Display S3: 170x320)
    static const uint16_t SCREEN_WIDTH = 170;
    static const uint16_t SCREEN_HEIGHT = 320;
    
    // Layout zones (portrait orientation)
    struct Layout {
        // Header
        uint16_t header_y;
        uint16_t header_h;
        
        // Waveform area
        uint16_t wave_y;
        uint16_t wave_h;
        
        // Numeric values
        uint16_t values_y;
        uint16_t values_h;
        
        // Status indicators
        uint16_t status_y;
        uint16_t status_h;
    } _layout;
    
    // Waveform buffer (circular buffer)
    static const uint16_t WAVEFORM_BUFFER_SIZE = 170;  // Full screen width
    uint16_t _waveformBuffer[WAVEFORM_BUFFER_SIZE];
    uint16_t _waveformIndex;
    uint16_t _waveformMin;
    uint16_t _waveformMax;
    
    // Display state
    uint32_t _lastUpdateTime;
    uint16_t _refreshRate;
    uint8_t _waveformSpeed;
    uint8_t _backlightBrightness;
    
    // Network info
    char _ssid[32];
    char _ip[16];
    
    // Previous values to avoid flicker
    struct PreviousValues {
        uint8_t fetco2;
        uint8_t fco2;
        float o2_percent;
        float volume_ml;
        uint8_t status2;
    } _prevValues;
    
    // Previous header title to avoid flicker
    char _prevTitle[32];
    
    // Drawing helper functions
    void drawHeader(const char* title);
    void drawWaveformArea();
    void drawNumericArea(const CO2Data& data);
    void drawStatusArea(const CO2Data& data);
    void drawMetricBox(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                       const char* label, const char* value, const char* unit,
                       uint16_t color = TFT_WHITE);
    void drawStatusBadge(uint16_t x, uint16_t y, const char* text, 
                        bool active, uint16_t activeColor = TFT_GREEN);
    
    // Waveform rendering
    void plotWaveform();
    void updateWaveformScale();
    
    // Color scheme
    uint16_t getValueColor(float value, float warning, float critical);
};

#endif // DISPLAY_MANAGER_H
