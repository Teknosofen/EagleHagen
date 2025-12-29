// ADCManager.cpp
// Implementation of ADC management for analog sensors

#include "ADCManager.h"

ADCManager::ADCManager(uint8_t o2_pin, uint8_t vol_pin)
    : _o2Pin(o2_pin)
    , _volPin(vol_pin)
    , _o2_raw(0)
    , _vol_raw(0)
    , _o2_voltage(0.0)
    , _vol_voltage(0.0)
    , _filterEnabled(true)
    , _filterSize(10)
    , _o2FilterBuffer(nullptr)
    , _volFilterBuffer(nullptr)
    , _filterIndex(0)
{
    // Default O2 calibration (0-3.3V = 0-100% O2, linear)
    _o2Cal.v_at_0_percent = 0.0;
    _o2Cal.v_at_100_percent = 3.3;
    
    // Default volume calibration (200 mL per volt, no offset)
    _volCal.ml_per_volt = 200.0;
    _volCal.offset_ml = 0.0;
}

bool ADCManager::begin() {
    Serial.println("Initializing ADC Manager...");
    
    // Configure ADC resolution and attenuation
    analogReadResolution(12);       // 12-bit resolution (0-4095)
    analogSetAttenuation(ADC_11db); // 0-3.3V range
    
    // Characterize ADC for better accuracy
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, 
                            ADC_WIDTH_BIT_12, 1100, &_adcChars);
    
    // Configure pins as inputs
    pinMode(_o2Pin, INPUT);
    pinMode(_volPin, INPUT);
    
    // Allocate filter buffers
    if (_filterEnabled) {
        _o2FilterBuffer = new uint16_t[_filterSize];
        _volFilterBuffer = new uint16_t[_filterSize];
        
        if (!_o2FilterBuffer || !_volFilterBuffer) {
            Serial.println("Failed to allocate filter buffers");
            return false;
        }
        
        // Initialize filter buffers with first reading
        uint16_t o2_init = readADC(_o2Pin);
        uint16_t vol_init = readADC(_volPin);
        
        for (int i = 0; i < _filterSize; i++) {
            _o2FilterBuffer[i] = o2_init;
            _volFilterBuffer[i] = vol_init;
        }
    }
    
    Serial.println("ADC Manager initialized");
    return true;
}

void ADCManager::update(CO2Data& data) {
    // Read raw ADC values
    uint16_t o2_new = readADC(_o2Pin);
    uint16_t vol_new = readADC(_volPin);
    
    // Apply filtering if enabled
    if (_filterEnabled && _o2FilterBuffer && _volFilterBuffer) {
        _o2_raw = applyFilter(_o2FilterBuffer, o2_new);
        _vol_raw = applyFilter(_volFilterBuffer, vol_new);
    } else {
        _o2_raw = o2_new;
        _vol_raw = vol_new;
    }
    
    // Convert to voltages
    _o2_voltage = rawToVoltage(_o2_raw);
    _vol_voltage = rawToVoltage(_vol_raw);
    
    // Convert to physical units
    data.o2_percent = voltageToO2Percent(_o2_voltage);
    data.volume_ml = voltageToVolume(_vol_voltage);
    
    // Scale to PIC-compatible format for LabVIEW
    data.o2_adc = scaleToPIC_AN0(_o2_raw);
    data.vol_adc = scaleToPIC_AN1(_vol_raw);
}

void ADCManager::setO2Calibration(float voltage_at_0_percent, float voltage_at_100_percent) {
    _o2Cal.v_at_0_percent = voltage_at_0_percent;
    _o2Cal.v_at_100_percent = voltage_at_100_percent;
    Serial.printf("O2 calibration set: 0%%=%0.3fV, 100%%=%0.3fV\n", 
                  voltage_at_0_percent, voltage_at_100_percent);
}

void ADCManager::setVolumeCalibration(float ml_per_volt, float offset_ml) {
    _volCal.ml_per_volt = ml_per_volt;
    _volCal.offset_ml = offset_ml;
    Serial.printf("Volume calibration set: %0.1f mL/V, offset=%0.1f mL\n",
                  ml_per_volt, offset_ml);
}

void ADCManager::setFilterSize(uint8_t size) {
    if (size < 1) size = 1;
    if (size > 50) size = 50;
    
    if (size != _filterSize) {
        // Reallocate buffers
        if (_o2FilterBuffer) delete[] _o2FilterBuffer;
        if (_volFilterBuffer) delete[] _volFilterBuffer;
        
        _filterSize = size;
        _o2FilterBuffer = new uint16_t[_filterSize];
        _volFilterBuffer = new uint16_t[_filterSize];
        _filterIndex = 0;
        
        // Initialize with current reading
        uint16_t o2_init = readADC(_o2Pin);
        uint16_t vol_init = readADC(_volPin);
        for (int i = 0; i < _filterSize; i++) {
            _o2FilterBuffer[i] = o2_init;
            _volFilterBuffer[i] = vol_init;
        }
    }
}

uint16_t ADCManager::readADC(uint8_t pin) {
    return analogRead(pin);
}

float ADCManager::rawToVoltage(uint16_t raw) {
    // Use ESP32 calibration for accurate voltage
    uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(raw, &_adcChars);
    return voltage_mv / 1000.0;  // Convert mV to V
}

float ADCManager::voltageToO2Percent(float voltage) {
    // Linear interpolation between calibration points
    float voltage_range = _o2Cal.v_at_100_percent - _o2Cal.v_at_0_percent;
    
    if (voltage_range <= 0.0) {
        return 0.0;  // Invalid calibration
    }
    
    float percent = ((voltage - _o2Cal.v_at_0_percent) / voltage_range) * 100.0;
    
    // Clamp to reasonable range
    if (percent < 0.0) percent = 0.0;
    if (percent > 100.0) percent = 100.0;
    
    return percent;
}

float ADCManager::voltageToVolume(float voltage) {
    return (voltage * _volCal.ml_per_volt) + _volCal.offset_ml;
}

uint16_t ADCManager::applyFilter(uint16_t* buffer, uint16_t newValue) {
    // Moving average filter
    buffer[_filterIndex] = newValue;
    _filterIndex = (_filterIndex + 1) % _filterSize;
    
    uint32_t sum = 0;
    for (int i = 0; i < _filterSize; i++) {
        sum += buffer[i];
    }
    
    return sum / _filterSize;
}

uint16_t ADCManager::scaleToPIC_AN0(uint16_t raw) {
    // PIC AN0 format: 10-bit left-justified to 16-bit (0-65535)
    // ESP32: 12-bit (0-4095) needs to scale to 16-bit
    return map(raw, 0, 4095, 0, 65535);
}

uint16_t ADCManager::scaleToPIC_AN1(uint16_t raw) {
    // PIC AN1 format: 10-bit right-justified (0-1023)
    // ESP32: 12-bit (0-4095) needs to scale down to 10-bit
    return map(raw, 0, 4095, 0, 1023);
}
