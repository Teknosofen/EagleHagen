// DataLogger.cpp
// Implementation of data logging and LabVIEW interface

#include "DataLogger.h"

DataLogger::DataLogger()
    : _labviewEnabled(true)
    , _csvEnabled(false)
    , _packetsSent(0)
    , _bytesSent(0)
{
}

bool DataLogger::begin() {
    Serial.println("DataLogger initialized");
    return true;
}

void DataLogger::sendPICFormat(Stream& stream, const CO2Data& data) {
    if (!_labviewEnabled) {
        return;
    }
    
    char buffer[64];
    formatPICPacket(buffer, sizeof(buffer), data);
    
    size_t written = stream.print(buffer);
    _bytesSent += written;
    _packetsSent++;
}

void DataLogger::setLabViewEnabled(bool enabled) {
    _labviewEnabled = enabled;
    Serial.printf("LabVIEW output %s\n", enabled ? "enabled" : "disabled");
}

void DataLogger::enableCSVLogging(bool enabled) {
    _csvEnabled = enabled;
    // TODO: Implement CSV file logging
    Serial.printf("CSV logging %s (not yet implemented)\n", 
                  enabled ? "enabled" : "disabled");
}

void DataLogger::resetStatistics() {
    _packetsSent = 0;
    _bytesSent = 0;
}

void DataLogger::formatPICPacket(char* buffer, size_t bufferSize, const CO2Data& data) {
    // Format matches original PIC output:
    // <ESC>ABC<TAB>DEFGH<TAB>IJKLM<TAB>[Status1][Status2][RR][FCO2][FetCO2]<CR><LF>
    //
    // Where:
    //  ABC    = CO2 waveform (3 digits, 0-255)
    //  DEFGH  = O2 ADC (5 digits, 0-65535)
    //  IJKLM  = Volume ADC (5 digits, 0-1023)
    //  Status1 = Status byte 1
    //  Status2 = Status byte 2 (with zero replacement)
    //  RR      = Respiratory rate (with zero replacement)
    //  FCO2    = Fractional CO2 (with zero replacement)
    //  FetCO2  = End-tidal CO2 (with zero replacement)
    
    snprintf(buffer, bufferSize,
        "\x1B%03d\t%05d\t%05d\t%c%c%c%c%c\r\n",
        data.co2_waveform,
        data.o2_adc,
        data.vol_adc,
        data.status1,
        replaceZero(data.status2, 128),
        replaceZero(data.respiratory_rate, 255),
        replaceZero(data.fco2, 255),
        replaceZero(data.fetco2, 255)
    );
}

uint8_t DataLogger::replaceZero(uint8_t value, uint8_t replacement) {
    // PIC firmware replaces zeros with specific values to avoid
    // null bytes in the data stream
    return (value == 0) ? replacement : value;
}
