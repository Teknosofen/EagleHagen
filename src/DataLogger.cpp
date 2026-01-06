// DataLogger.cpp
// Implementation of data logging with multiple output formats

#include "DataLogger.h"

DataLogger::DataLogger()
    : _outputFormat(FORMAT_TAB_SEPARATED)
    , _outputEnabled(true)
    , _csvEnabled(false)
    , _packetsSent(0)
    , _bytesSent(0)
{
}

bool DataLogger::begin() {
    Serial.println("DataLogger initialized");
    Serial.println("Tab-separated ASCII output enabled (default)");
    return true;
}

void DataLogger::sendData(Stream& stream, const CO2Data& data) {
    if (!_outputEnabled) {
        return;
    }
    
    switch (_outputFormat) {
        case FORMAT_LEGACY_LABVIEW:
            sendPICFormat(stream, data);
            break;
        case FORMAT_TAB_SEPARATED:
            sendTabSeparated(stream, data);
            break;
    }
}

void DataLogger::sendPICFormat(Stream& stream, const CO2Data& data) {
    char buffer[64];
    formatPICPacket(buffer, sizeof(buffer), data);
    
    size_t written = stream.print(buffer);
    stream.flush();  // Ensure data is sent immediately
    
    _bytesSent += written;
    _packetsSent++;
}

void DataLogger::sendTabSeparated(Stream& stream, const CO2Data& data) {
    // New format: Tab-separated ASCII
    // Status1<TAB>Status2<TAB>RR<TAB>FCO2<TAB>FetCO2<TAB>O2%<TAB>Volume_mL<CR><LF>

    char buffer[96];
    snprintf(buffer, sizeof(buffer),
        "%d\t%d\t%d\t%d\t%d\t%.1f\t%.1f\r\n",
        data.status1,
        data.status2,
        data.respiratory_rate,
        data.co2_waveform,  // FCO2 curve (d[4])
        data.fetco2,        // FetCO2 peak (d[5])
        data.o2_percent,    // O2 percentage
        data.volume_ml      // Volume in mL
    );

    size_t written = stream.print(buffer);
    stream.flush();

    _bytesSent += written;
    _packetsSent++;
}

void DataLogger::setOutputEnabled(bool enabled) {
    _outputEnabled = enabled;
    Serial.printf("Host output %s\n", enabled ? "enabled" : "disabled");
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
