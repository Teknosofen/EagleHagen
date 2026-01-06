// MaCO2Parser.h
// Handles communication protocol with MedAir MaCO2-V3 sensor
// Parses 8-byte data packets and manages initialization

#ifndef MACO2_PARSER_H
#define MACO2_PARSER_H

#include <Arduino.h>

// MaCO2 sensor raw packet structure (8 bytes)
// FINAL STRUCTURE based on actual sensor data analysis with checksum validation
//
// Byte | Field      | Range     | Description
// -----|------------|-----------|------------------------------------------
// d[0] | status1    | 6         | Data valid flag (always 0x06 for valid data)
// d[1] | status2    | 0-15      | Status byte 2 (pump, leak, occlusion bits)
// d[2] | rr         | 0-60      | Respiratory Rate (breaths/min)
// d[3] | fico2      | 0-3       | FiCO2 - Fractional Inspired CO2 (baseline)
// d[4] | fco2_wave  | 0-32      | FCO2 - Real-time CO2 waveform (8Hz updates)
// d[5] | fetco2     | 0-120     | FetCO2 - End-Tidal CO2 (peak, held between breaths)
// d[6] | reserved1  | varies    | Reserved/Unknown
// d[7] | checksum   | 0-255     | Checksum: sum(d[0..6]) & 0xFF
//
// Sync Strategy:
// - Look for 0x06 header (d[0])
// - Validate checksum (d[7] must equal sum of d[0..6])
// - Validate RR (d[2] must be <= 60)
//
struct MaCO2Packet {
    uint8_t status1;        // d[0] - Status/Data Valid (6 = valid data)
    uint8_t status2;        // d[1] - Status byte 2  
    uint8_t rr;             // d[2] - Respiratory Rate (breaths/min)
    uint8_t fico2;          // d[3] - Fractional Inspired CO2 (baseline ~0-3)
    uint8_t fco2_wave;      // d[4] - CO2 Waveform (0-32 mmHg typical)
    uint8_t fetco2;         // d[5] - End-Tidal CO2 (peak value ~0-120)
    uint8_t reserved1;      // d[6] - Reserved/Unknown
    uint8_t checksum;       // d[7] - Checksum: sum of d[0..6] & 0xFF
};

// Complete system data (MaCO2 + ADC readings)
struct CO2Data {
    // From MaCO2 sensor
    uint16_t co2_waveform;      // fetco2 value (0-255)
    uint8_t status1;            // Data valid flag
    uint8_t status2;            // Pump, leak, occlusion bits
    uint8_t respiratory_rate;   // RR in breaths/min
    uint8_t fco2;               // Fractional CO2
    uint8_t fetco2;             // End-tidal CO2
    
    // From ADC (added by system)
    uint16_t o2_adc;            // O2 sensor reading (0-65535)
    uint16_t vol_adc;           // Volume sensor reading (0-1023)
    
    // Calculated values
    float o2_percent;           // Calculated O2 percentage
    float volume_ml;            // Calculated volume in mL
    
    // Metadata
    uint32_t timestamp;         // millis() when data was received
    bool valid;                 // Overall data validity
};

// MaCO2 command definitions
enum MaCO2Command : uint8_t {
    CMD_START_PUMP = 0xA5,
    CMD_ZERO_CAL = 0x5A
};

class MaCO2Parser {
public:
    MaCO2Parser();
    
    // Initialize communication with MaCO2 sensor
    bool initialize(HardwareSerial& serial, unsigned long timeout_ms = 10000);
    
    // Parse incoming data packet (non-blocking)
    bool parsePacket(HardwareSerial& serial, CO2Data& data);
    
    // Send command to MaCO2 sensor
    void sendCommand(HardwareSerial& serial, MaCO2Command cmd);
    
    // Check status flags
    bool isPumpRunning(const CO2Data& data) const;
    bool isLeakDetected(const CO2Data& data) const;
    bool isOcclusionDetected(const CO2Data& data) const;
    bool isDataValid(const CO2Data& data) const;
    
    // Get packet statistics
    uint32_t getPacketCount() const { return _packetCount; }
    uint32_t getErrorCount() const { return _errorCount; }
    uint32_t getLastPacketTime() const { return _lastPacketTime; }
    
    // Reset statistics
    void resetStatistics();
    
private:
    enum ParseState {
        WAIT_FOR_DATA,
        READING_PACKET
    };
    
    ParseState _state;
    MaCO2Packet _rxBuffer;
    uint8_t _rxIndex;
    uint32_t _packetCount;
    uint32_t _errorCount;
    uint32_t _lastPacketTime;
    
    bool readPacket(HardwareSerial& serial);
    void decodePacket(const MaCO2Packet& packet, CO2Data& data);
};

#endif // MACO2_PARSER_H
