// DataLogger.h
// Handles data export in PIC-compatible format for LabVIEW
// Also manages CSV logging and data recording

#ifndef DATA_LOGGER_H
#define DATA_LOGGER_H

#include <Arduino.h>
#include "MaCO2Parser.h"  // For CO2Data structure

class DataLogger {
public:
    DataLogger();
    
    // Initialize logger
    bool begin();
    
    // Send data in PIC-compatible format to stream (for LabVIEW)
    void sendPICFormat(Stream& stream, const CO2Data& data);
    
    // Enable/disable LabVIEW mirroring via USB CDC
    void setLabViewEnabled(bool enabled);
    bool isLabViewEnabled() const { return _labviewEnabled; }
    
    // CSV logging (future implementation)
    void enableCSVLogging(bool enabled);
    bool isCSVLoggingEnabled() const { return _csvEnabled; }
    
    // Get statistics
    uint32_t getPacketsSent() const { return _packetsSent; }
    uint32_t getBytesSent() const { return _bytesSent; }
    
    // Reset statistics
    void resetStatistics();
    
private:
    bool _labviewEnabled;
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
