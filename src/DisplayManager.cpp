// DisplayManager.cpp
// Implementation of TFT display management for LilyGO T-Display S3

#include "DisplayManager.h"

// Layout constants for status section
static const uint8_t STATUS_SEPARATOR_Y = 24;
static const uint8_t SSID_Y_OFFSET = 35;
static const uint8_t IP_Y_OFFSET = 48;

DisplayManager::DisplayManager()
    : _waveformIndex(0)
    , _waveformMin(0)
    , _waveformMax(100)
    , _lastUpdateTime(0)
    , _refreshRate(50)
    , _waveformSpeed(2)
    , _backlightBrightness(200)
{
    memset(_waveformBuffer, 0, sizeof(_waveformBuffer));
    
    // Initialize network info
    memset(_ssid, 0, sizeof(_ssid));
    memset(_ip, 0, sizeof(_ip));
    
    // Initialize previous values
    _prevValues.fetco2 = 255;
    _prevValues.fco2 = 255;
    _prevValues.o2_percent = -1;
    _prevValues.volume_ml = -1;
    _prevValues.status2 = 255;
    memset(_prevValues.etco2_str, 0, sizeof(_prevValues.etco2_str));
    memset(_prevValues.fco2_str, 0, sizeof(_prevValues.fco2_str));
    memset(_prevValues.o2_str, 0, sizeof(_prevValues.o2_str));
    memset(_prevValues.vol_str, 0, sizeof(_prevValues.vol_str));
    
    // Initialize previous title
    memset(_prevTitle, 0, sizeof(_prevTitle));
    
    // Define layout zones (portrait orientation: 170x320)
    _layout.header_y = 0;
    _layout.header_h = 30;  // Header section height
    
    _layout.wave_y = 30;  // Waveform starts after header
    _layout.wave_h = 135;  // Increased by 5 pixels (was 130)
    
    _layout.values_y = 165;  // Values moved DOWN by 5 pixels (was 160)
    _layout.values_h = 100;
    
    _layout.status_y = 265;  // Status moved DOWN by 5 pixels (was 260)
    _layout.status_h = 65;   // Reduced from 75 to 65
}

bool DisplayManager::begin() {
    Serial.println("Initializing TFT display...");
    
    // Enable display power (GPIO 15 controls display power on T-Display S3)
    pinMode(15, OUTPUT);
    digitalWrite(15, HIGH);
    delay(100);  // Wait for power to stabilize
    
    _tft.init();
    _tft.setRotation(2);  // Portrait mode (180° rotated)
    _tft.fillScreen(TFT_BLACK);
    
    // Set backlight
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);  // Turn on immediately
    setBacklight(_backlightBrightness);
    
    Serial.println("TFT display initialized");
    return true;
}

void DisplayManager::showSplash(const char* title, const char* subtitle) {
    _tft.fillScreen(TFT_LOGOBACKGROUND);
    _tft.setTextColor(TFT_DARKERBLUE, TFT_LOGOBACKGROUND);
    _tft.setTextDatum(MC_DATUM);
    
    // Title with FreeSans font for better appearance
    _tft.setFreeFont(&FreeSansBold12pt7b);
    _tft.drawString(title, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 - 10);
    _tft.setTextFont(1);
    
    // Subtitle in lighter color
    if (subtitle) {
        _tft.setTextColor(TFT_SLATEBLUE, TFT_LOGOBACKGROUND);
        _tft.setTextSize(1);
        _tft.drawString(subtitle, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 + 20);
    }
}

void DisplayManager::clearScreen() {
    _tft.fillScreen(TFT_LOGOBACKGROUND);
}

void DisplayManager::setNetworkInfo(const char* ssid, const char* ip) {
    if (ssid) {
        strncpy(_ssid, ssid, sizeof(_ssid) - 1);
        _ssid[sizeof(_ssid) - 1] = '\0';
    }
    if (ip) {
        strncpy(_ip, ip, sizeof(_ip) - 1);
        _ip[sizeof(_ip) - 1] = '\0';
    }
}

void DisplayManager::updateAll(const CO2Data& data) {
    // Throttle updates based on refresh rate
    if (millis() - _lastUpdateTime < _refreshRate) {
        return;
    }
    _lastUpdateTime = millis();
    
    // Draw all sections (header only redraws if title changed)
    drawHeader("Ornhagen");
    updateWaveform(data);
    updateNumericValues(data);
    updateStatusIndicators(data);
}

void DisplayManager::updateWaveform(const CO2Data& data) {
    // Clear waveform area completely each time
    _tft.fillRect(0, _layout.wave_y, SCREEN_WIDTH, _layout.wave_h, TFT_LOGOBACKGROUND);
    
    // Draw label
    _tft.setTextColor(TFT_SLATEBLUE, TFT_LOGOBACKGROUND);
    _tft.setTextDatum(BL_DATUM);
    _tft.setTextSize(1);
    _tft.drawString("CO2 Waveform", 5, _layout.wave_y + _layout.wave_h - 2);
    
    // Draw waveform
    plotWaveform();
}

void DisplayManager::updateNumericValues(const CO2Data& data) {
    const uint16_t y_start = _layout.values_y;
    const uint16_t col_width = SCREEN_WIDTH / 2;  // Two columns instead of three
    
    // Create temporary char buffers for string conversion
    char etco2_str[8];
    char fco2_str[8];
    char o2_str[8];
    char vol_str[8];
    
    // Convert CO2 values from mmHg to kPa (1 mmHg = 0.133322 kPa)
    float etco2_kpa = (float)data.fetco2 * 0.133322;
    float fco2_kpa = (float)data.fco2 * 0.133322;
    
    // Convert values to strings with one decimal place for CO2
    snprintf(etco2_str, sizeof(etco2_str), "%.1f", etco2_kpa);
    snprintf(fco2_str, sizeof(fco2_str), "%.1f", fco2_kpa);
    snprintf(o2_str, sizeof(o2_str), "%.1f", data.o2_percent);
    snprintf(vol_str, sizeof(vol_str), "%d", (int)data.volume_ml);
    
    // First row: EtCO2, FCO2 (removed RR to make space for decimals)
    // Only update if value changed
    if (data.fetco2 != _prevValues.fetco2) {
        drawMetricBox(0, y_start, col_width, 50,
                      "EtCO2", _prevValues.etco2_str, (const char*)etco2_str, "kPa",
                      TFT_DARKERBLUE);  // Darker blue for CO2
        _prevValues.fetco2 = data.fetco2;
        strncpy(_prevValues.etco2_str, etco2_str, sizeof(_prevValues.etco2_str));
    }
    
    if (data.fco2 != _prevValues.fco2) {
        drawMetricBox(col_width, y_start, col_width, 50,
                      "FCO2", _prevValues.fco2_str, (const char*)fco2_str, "kPa",
                      TFT_DARKERBLUE);  // Darker blue for CO2
        _prevValues.fco2 = data.fco2;
        strncpy(_prevValues.fco2_str, fco2_str, sizeof(_prevValues.fco2_str));
    }
    
    // Second row: O2, Volume
    if (abs(data.o2_percent - _prevValues.o2_percent) > 0.05) {  // Update if changed by > 0.05%
        drawMetricBox(0, y_start + 50, col_width, 50,
                      "O2", _prevValues.o2_str, (const char*)o2_str, "%",
                      TFT_SLATEBLUE);  // Slate blue for O2
        _prevValues.o2_percent = data.o2_percent;
        strncpy(_prevValues.o2_str, o2_str, sizeof(_prevValues.o2_str));
    }
    
    if (abs(data.volume_ml - _prevValues.volume_ml) > 0.5) {  // Update if changed by > 0.5mL
        drawMetricBox(col_width, y_start + 50, col_width, 50,
                      "Volume", _prevValues.vol_str, (const char*)vol_str, "mL",
                      TFT_SLATEBLUE);  // Slate blue for Volume
        _prevValues.volume_ml = data.volume_ml;
        strncpy(_prevValues.vol_str, vol_str, sizeof(_prevValues.vol_str));
    }
}

void DisplayManager::updateStatusIndicators(const CO2Data& data) {
    // Only update if status changed
    if (data.status2 == _prevValues.status2) {
        return;
    }
    _prevValues.status2 = data.status2;
    
    const uint16_t y_start = _layout.status_y;
    
    // Clear area with background color
    _tft.fillRect(0, y_start, SCREEN_WIDTH, _layout.status_h, TFT_LOGOBACKGROUND);
    
    // Status badges - very compact at top
    bool pump_running = (data.status2 & 0x01) != 0;
    bool leak = (data.status2 & 0x02) != 0;
    bool occlusion = (data.status2 & 0x04) != 0;
    
    const uint16_t badge_y = y_start + 1;  // Minimal top margin
    const uint16_t badge_spacing = SCREEN_WIDTH / 3;
    
    drawStatusBadge(badge_spacing * 0 + 5, badge_y, "PUMP", pump_running, TFT_GREENISH_TINT);
    drawStatusBadge(badge_spacing * 1 + 5, badge_y, "LEAK", !leak, TFT_GREENISH_TINT);
    drawStatusBadge(badge_spacing * 2 + 5, badge_y, "OCCL", !occlusion, TFT_GREENISH_TINT);
    
    // Thin separator line
    _tft.drawFastHLine(5, y_start + STATUS_SEPARATOR_Y, SCREEN_WIDTH - 10, TFT_MIDNIGHTBLUE);
    
    // Network info with labels - very tight spacing
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextSize(1);
    
    // SSID with label
    if (strlen(_ssid) > 0) {
        char ssid_label[40];
        snprintf(ssid_label, sizeof(ssid_label), "SSID: %s", _ssid);
        _tft.setTextColor(TFT_DARKERBLUE, TFT_LOGOBACKGROUND);
        _tft.drawString(ssid_label, SCREEN_WIDTH / 2, y_start + SSID_Y_OFFSET);
    }
    
    // IP with label
    if (strlen(_ip) > 0) {
        char ip_label[30];
        snprintf(ip_label, sizeof(ip_label), "IP: %s", _ip);
        _tft.setTextColor(TFT_SLATEBLUE, TFT_LOGOBACKGROUND);
        _tft.drawString(ip_label, SCREEN_WIDTH / 2, y_start + IP_Y_OFFSET);
    }
}

void DisplayManager::addWaveformPoint(uint16_t co2_value) {
    // Add point to circular buffer
    _waveformBuffer[_waveformIndex] = co2_value;
    _waveformIndex = (_waveformIndex + 1) % WAVEFORM_BUFFER_SIZE;
    
    // Update scale
    updateWaveformScale();
}

void DisplayManager::setBacklight(uint8_t brightness) {
    _backlightBrightness = brightness;
    analogWrite(TFT_BL, brightness);
}

void DisplayManager::backlightOn() {
    setBacklight(_backlightBrightness);
}

void DisplayManager::backlightOff() {
    setBacklight(0);
}

void DisplayManager::setWaveformSpeed(uint8_t speed) {
    if (speed >= 1 && speed <= 10) {
        _waveformSpeed = speed;
    }
}

void DisplayManager::setRefreshRate(uint16_t rate_ms) {
    _refreshRate = rate_ms;
}

// Private helper functions

void DisplayManager::drawHeader(const char* title) {
    // Only redraw if title has changed
    if (strcmp(_prevTitle, title) == 0) {
        return;  // Title unchanged, skip redraw to avoid flicker
    }
    
    // Clear header area with soft background
    _tft.fillRect(0, 0, SCREEN_WIDTH, _layout.header_h, TFT_LOGOBACKGROUND);
    
    // Draw title - centered with smooth FreeSans font
    _tft.setTextColor(TFT_DEEPBLUE, TFT_LOGOBACKGROUND);
    _tft.setTextDatum(MC_DATUM);  // Middle Center
    _tft.setFreeFont(&FreeSansBold12pt7b);  // Use built-in smooth font
    _tft.drawString(title, SCREEN_WIDTH / 2, 12);  // Y=12 for more compact header
    _tft.setTextFont(1);  // Reset to default font
    
    // Draw WiFi indicator (stronger green dot on right)
    _tft.fillCircle(SCREEN_WIDTH - 10, _layout.header_h / 2, 4, TFT_STRONGER_GREEN);
    
    // Save title for next comparison
    strncpy(_prevTitle, title, sizeof(_prevTitle) - 1);
    _prevTitle[sizeof(_prevTitle) - 1] = '\0';
}

void DisplayManager::plotWaveform() {
    const uint16_t wave_x = 5;
    const uint16_t wave_y = _layout.wave_y + 10;
    const uint16_t wave_w = SCREEN_WIDTH - 10;
    const uint16_t wave_h = _layout.wave_h - 30;
    
    // Draw grid lines
    _tft.drawFastHLine(wave_x, wave_y + wave_h / 4, wave_w, TFT_MIDNIGHTBLUE);
    _tft.drawFastHLine(wave_x, wave_y + wave_h / 2, wave_w, TFT_MIDNIGHTBLUE);
    _tft.drawFastHLine(wave_x, wave_y + wave_h * 3 / 4, wave_w, TFT_MIDNIGHTBLUE);
    
    // Calculate scaling
    uint16_t range = (_waveformMax - _waveformMin);
    if (range == 0) range = 1;
    
    // Draw simple line graph
    for (int i = 1; i < WAVEFORM_BUFFER_SIZE; i++) {
        int idx = (_waveformIndex + i) % WAVEFORM_BUFFER_SIZE;
        int prev_idx = (_waveformIndex + i - 1) % WAVEFORM_BUFFER_SIZE;
        
        // Scale to screen coordinates
        uint16_t x1 = wave_x + ((i - 1) * wave_w / WAVEFORM_BUFFER_SIZE);
        uint16_t x2 = wave_x + (i * wave_w / WAVEFORM_BUFFER_SIZE);
        
        uint16_t y1 = wave_y + wave_h - 
                     ((_waveformBuffer[prev_idx] - _waveformMin) * wave_h / range);
        uint16_t y2 = wave_y + wave_h - 
                     ((_waveformBuffer[idx] - _waveformMin) * wave_h / range);
        
        // Clamp to bounds
        y1 = constrain(y1, wave_y, wave_y + wave_h);
        y2 = constrain(y2, wave_y, wave_y + wave_h);
        
        // Draw single line segment
        _tft.drawLine(x1, y1, x2, y2, TFT_DARKERBLUE);
    }
}

void DisplayManager::updateWaveformScale() {
    // Find min and max in buffer
    uint16_t min_val = 0xFFFF;
    uint16_t max_val = 0;
    
    for (int i = 0; i < WAVEFORM_BUFFER_SIZE; i++) {
        if (_waveformBuffer[i] < min_val) min_val = _waveformBuffer[i];
        if (_waveformBuffer[i] > max_val) max_val = _waveformBuffer[i];
    }
    
    // Add some margin
    _waveformMin = (min_val > 5) ? min_val - 5 : 0;
    _waveformMax = max_val + 10;
    
    // Ensure minimum range
    if (_waveformMax - _waveformMin < 20) {
        _waveformMax = _waveformMin + 20;
    }
}

void DisplayManager::drawMetricBox(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                   const char* label, const char* old_value, const char* new_value,
                                   const char* unit, uint16_t color) {
    // Draw border in soft color
    _tft.drawRect(x + 1, y + 1, w - 2, h - 2, TFT_MIDNIGHTBLUE);
    
    // Label in soft color
    _tft.setTextColor(TFT_SLATEBLUE, TFT_LOGOBACKGROUND);
    _tft.setTextDatum(TC_DATUM);
    _tft.setTextSize(1);
    _tft.drawString(label, x + w / 2, y + 5);
    
    // Erase old value by drawing it in background color
    if (old_value && strlen(old_value) > 0) {
        _tft.setTextColor(TFT_LOGOBACKGROUND, TFT_LOGOBACKGROUND);
        _tft.setTextDatum(MC_DATUM);
        _tft.setTextSize(2);
        _tft.drawString(old_value, x + w / 2, y + h / 2 + 5);
    }
    
    // Draw new value in provided color
    _tft.setTextColor(color, TFT_LOGOBACKGROUND);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextSize(2);
    _tft.drawString(new_value, x + w / 2, y + h / 2 + 5);
    
    // Unit in soft grey
    _tft.setTextColor(TFT_DARKERBLUE, TFT_LOGOBACKGROUND);
    _tft.setTextDatum(BC_DATUM);
    _tft.setTextSize(1);
    _tft.drawString(unit, x + w / 2, y + h - 3);
}

void DisplayManager::drawStatusBadge(uint16_t x, uint16_t y, const char* text,
                                     bool active, uint16_t activeColor) {
    const uint16_t badge_w = 50;
    const uint16_t badge_h = 20;  // Reduced from 30 to 20
    
    // Background - softer colors
    uint16_t bg_color = active ? TFT_GREENISH_TINT : TFT_REDDISH_TINT;
    uint16_t text_color = active ? TFT_BLACK : TFT_DEEPBLUE;
    
    _tft.fillRoundRect(x, y, badge_w, badge_h, 4, bg_color);
    _tft.drawRoundRect(x, y, badge_w, badge_h, 4, active ? TFT_GREENISH_TINT : TFT_REDDISH_TINT);
    
    // Status indicator (circle) - smaller
    uint16_t circle_color = active ? TFT_DEEPBLUE : TFT_DARKERBLUE;
    _tft.fillCircle(x + 8, y + badge_h / 2, 3, circle_color);
    
    // Text
    _tft.setTextColor(text_color, bg_color);
    _tft.setTextDatum(MC_DATUM);
    _tft.setTextSize(1);
    _tft.drawString(text, x + badge_w / 2 + 3, y + badge_h / 2);
}

uint16_t DisplayManager::getValueColor(float value, float warning, float critical) {
    if (value >= critical) return TFT_RED;
    if (value >= warning) return TFT_YELLOW;
    return TFT_GREEN;
}
