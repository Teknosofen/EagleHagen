// DataLogger.h
// Handles data export in multiple formats:
// - Legacy PIC format (for LabVIEW compatibility)
// - Tab-separated ASCII format (for modern tools)

#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <Arduino.h>
#include "MaCO2Parser.h"  // For CO2Data structure

// Output format selection
enum OutputFormat {
    FORMAT_LEGACY_LABVIEW = 0,  // Original PIC format with binary data
    FORMAT_TAB_SEPARATED = 1     // Tab-separated ASCII format
};

class DataLogger {
public:
    DataLogger();
    
    // Initialize logger
    bool begin();
    
    // Send data to stream in selected format (8Hz)
    void sendData(Stream& stream, const CO2Data& data);
    
    // Legacy PIC-compatible format (for LabVIEW)
    void sendPICFormat(Stream& stream, const CO2Data& data);
    
    // New tab-separated ASCII format
    void sendTabSeparated(Stream& stream, const CO2Data& data);
    
    // Format selection
    void setOutputFormat(OutputFormat format) { _outputFormat = format; }
    OutputFormat getOutputFormat() const { return _outputFormat; }
    
    // Enable/disable host output via USB CDC
    void setOutputEnabled(bool enabled);
    bool isOutputEnabled() const { return _outputEnabled; }
    
    // CSV logging (future implementation)
    void enableCSVLogging(bool enabled);
    bool isCSVLoggingEnabled() const { return _csvEnabled; }
    
    // Get statistics
    uint32_t getPacketsSent() const { return _packetsSent; }
    uint32_t getBytesSent() const { return _bytesSent; }
    
    // Reset statistics
    void resetStatistics();
    
private:
    OutputFormat _outputFormat;
    bool _outputEnabled;
    bool _csvEnabled;
    uint32_t _packetsSent;
    uint32_t _bytesSent;
    
    // Format data packet in original PIC format
    // Format: <ESC>ABC<TAB>DEFGH<TAB>IJKLM<TAB>[Status1][Status2][RR][FiCO2][FetCO2]<CR><LF>
    void formatPICPacket(char* buffer, size_t bufferSize, const CO2Data& data);
    
    // Handle zero replacement (PIC firmware compatibility)
    uint8_t replaceZero(uint8_t value, uint8_t replacement);
};

#endif // DATA_LOGGER_H
