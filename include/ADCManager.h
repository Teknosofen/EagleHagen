// ADCManager.h
// Manages ESP32 onboard ADC for O2 and Volume sensors
// Handles calibration, filtering, and conversion to physical units

#ifndef ADC_MANAGER_H
#define ADC_MANAGER_H

#include <Arduino.h>
#include <esp_adc_cal.h>
#include "MaCO2Parser.h"  // For CO2Data structure

class ADCManager {
public:
    ADCManager(uint8_t o2_pin, uint8_t vol_pin);
    
    // Initialize ADC with calibration
    bool begin();
    
    // Read and update ADC values in CO2Data structure
    void update(CO2Data& data);
    
    // Set calibration parameters for O2 sensor
    // voltage_at_0_percent: voltage reading at 0% O2
    // voltage_at_100_percent: voltage reading at 100% O2 (typically air = 20.9%)
    void setO2Calibration(float voltage_at_0_percent, float voltage_at_100_percent);
    
    // Set calibration parameters for volume sensor
    // ml_per_volt: conversion factor (mL per volt)
    // offset_ml: offset in mL (reading at 0V)
    void setVolumeCalibration(float ml_per_volt, float offset_ml);
    
    // Get raw ADC readings (for diagnostics)
    uint16_t getO2Raw() const { return _o2_raw; }
    uint16_t getVolRaw() const { return _vol_raw; }
    
    // Get voltages (for diagnostics)
    float getO2Voltage() const { return _o2_voltage; }
    float getVolVoltage() const { return _vol_voltage; }
    
    // Enable/disable moving average filtering
    void setFilterEnabled(bool enabled) { _filterEnabled = enabled; }
    void setFilterSize(uint8_t size);
    
private:
    // Pin assignments
    uint8_t _o2Pin;
    uint8_t _volPin;
    
    // ADC calibration
    esp_adc_cal_characteristics_t _adcChars;
    
    // Current readings
    uint16_t _o2_raw;
    uint16_t _vol_raw;
    float _o2_voltage;
    float _vol_voltage;
    
    // Calibration parameters
    struct {
        float v_at_0_percent;
        float v_at_100_percent;
    } _o2Cal;
    
    struct {
        float ml_per_volt;
        float offset_ml;
    } _volCal;
    
    // Moving average filter
    bool _filterEnabled;
    uint8_t _filterSize;
    uint16_t* _o2FilterBuffer;
    uint16_t* _volFilterBuffer;
    uint8_t _filterIndex;
    
    // Helper functions
    uint16_t readADC(uint8_t pin);
    float rawToVoltage(uint16_t raw);
    float voltageToO2Percent(float voltage);
    float voltageToVolume(float voltage);
    uint16_t applyFilter(uint16_t* buffer, uint16_t newValue);
    
    // Scale ADC readings to match PIC format for LabVIEW compatibility
    uint16_t scaleToPIC_AN0(uint16_t raw);  // 12-bit to 16-bit (left-justified)
    uint16_t scaleToPIC_AN1(uint16_t raw);  // 12-bit to 10-bit (right-justified)
};

#endif // ADC_MANAGER_H
