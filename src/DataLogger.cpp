// DataLogger.cpp
// Implementation of data logging with multiple output formats

#include "DataLogger.h"
#include "DebugLog.h"

DataLogger::DataLogger()
    : _outputFormat(FORMAT_LEGACY_LABVIEW)
    , _outputEnabled(true)
    , _csvEnabled(false)
    , _packetsSent(0)
    , _bytesSent(0)
{
}

bool DataLogger::begin() {
    updateDebugOutput();
    DebugOut.println("DataLogger initialized");
    DebugOut.printf("Host output format: %s\n",
                    _outputFormat == FORMAT_LEGACY_LABVIEW ? "Legacy LabVIEW" : "Tab-separated ASCII");
    return true;
}

void DataLogger::setOutputFormat(OutputFormat format) {
    _outputFormat = format;
    updateDebugOutput();
}

void DataLogger::updateDebugOutput() {
    // Diagnostics would corrupt the fixed-length LabVIEW frames on the shared USB port
    DebugOut.setEnabled(!_outputEnabled || _outputFormat != FORMAT_LEGACY_LABVIEW);
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
    updateDebugOutput();
    DebugOut.printf("Host output %s\n", enabled ? "enabled" : "disabled");
}

void DataLogger::enableCSVLogging(bool enabled) {
    _csvEnabled = enabled;
    // TODO: Implement CSV file logging
    DebugOut.printf("CSV logging %s (not yet implemented)\n", 
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
    // Where (units as expected by the existing LabVIEW program, unchanged since the PIC):
    //  ABC    = CO2 waveform in mmHg (3 digits); LabVIEW divides by 7.60 to get %
    //  DEFGH  = O2 in 0.1 % (5 digits, 209 = 20.9 %); LabVIEW divides by 10
    //  IJKLM  = Volume ADC (5 digits, 0-1023); LabVIEW divides by ADC_counts/liter
    //  Status1 = Status byte 1 (6 = data valid)
    //  Status2 = Status byte 2 (with zero replacement)
    //  RR      = Respiratory rate (with zero replacement)
    //  FiCO2   = Inspired CO2 in mmHg (byte, with zero replacement)
    //  FetCO2  = End-tidal CO2 in mmHg (byte, with zero replacement)
    //
    // Frame is always 24 bytes; LabVIEW reads fixed 24-byte blocks.

    snprintf(buffer, bufferSize,
        "\x1B%03d\t%05d\t%05d\t%c%c%c%c%c\r\n",
        (int)data.co2_waveform,
        (int)(data.o2_percent * 10.0f),
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
