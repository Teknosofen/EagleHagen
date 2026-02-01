// DataLogger.cpp
// Implementation of data logging with multiple output formats

#include "DataLogger.h"

DataLogger::DataLogger()
    : _outputFormat(FORMAT_LEGACY_LABVIEW)
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
    // Tab-separated ASCII format (CO2 in kPa, matching web interface)
    // CO2_kPa<TAB>O2%<TAB>RR<TAB>Volume_mL<TAB>Status1<TAB>Status2<CR><LF>

    // Convert CO2 from mmHg to kPa (1 mmHg = 0.133322 kPa)
    float co2_kpa = data.co2_waveform * 0.133322f;

    char buffer[96];
    snprintf(buffer, sizeof(buffer),
        "%.1f\t%.1f\t%d\t%d\t%d\t%d\r\n",
        co2_kpa,              // CO2 waveform in kPa
        data.o2_percent,      // O2 percentage
        data.respiratory_rate,
        (int)data.volume_ml,  // Volume in mL
        data.status1,
        data.status2
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
    //  ABC    = CO2 waveform scaled (3 digits, 53 = 5.3 kPa)
    //  DEFGH  = O2 scaled (5 digits, 201 = 20.1%)
    //  IJKLM  = Volume ADC (5 digits, 0-1023)
    //  Status1 = Status byte 1
    //  Status2 = Status byte 2 (with zero replacement)
    //  RR      = Respiratory rate (with zero replacement)
    //  FCO2    = FiCO2 scaled (byte, 4 = 0.4 kPa)
    //  FetCO2  = End-tidal CO2 scaled (byte, 53 = 5.3 kPa)
    //
    // All CO2 fields: mmHg * 0.133322 * 10 = kPa * 10 (one implicit decimal)

    int co2_scaled    = (int)(data.co2_waveform * 1.33322f);  // mmHg → kPa * 10
    uint8_t fco2_scaled  = (uint8_t)(data.fco2  * 1.33322f);  // mmHg → kPa * 10
    uint8_t fetco2_scaled = (uint8_t)(data.fetco2 * 1.33322f); // mmHg → kPa * 10

    snprintf(buffer, bufferSize,
        "\x1B%03d\t%05d\t%05d\t%c%c%c%c%c\r\n",
        co2_scaled,
        (int)(data.o2_percent * 10.0f),
        data.vol_adc,
        data.status1,
        replaceZero(data.status2, 128),
        replaceZero(data.respiratory_rate, 255),
        replaceZero(fco2_scaled, 255),
        replaceZero(fetco2_scaled, 255)
    );
}

uint8_t DataLogger::replaceZero(uint8_t value, uint8_t replacement) {
    // PIC firmware replaces zeros with specific values to avoid
    // null bytes in the data stream
    return (value == 0) ? replacement : value;
}
