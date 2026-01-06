// MaCO2Parser.cpp
// Implementation of MaCO2 sensor communication protocol

#include "MaCO2Parser.h"

MaCO2Parser::MaCO2Parser() 
    : _state(WAIT_FOR_DATA)
    , _rxIndex(0)
    , _packetCount(0)
    , _errorCount(0)
    , _lastPacketTime(0)
{
    memset(&_rxBuffer, 0, sizeof(_rxBuffer));
}

bool MaCO2Parser::initialize(HardwareSerial& serial, unsigned long timeout_ms) {
    Serial.println("Initializing MaCO2 sensor...");
    
    // Flush any old data
    while (serial.available()) {
        serial.read();
    }
    delay(100);
    
    unsigned long startTime = millis();
    
    // Wait for start byte (0x06) from MaCO2
    while (millis() - startTime < timeout_ms) {
        if (serial.available()) {
            uint8_t byte = serial.read();
            Serial.printf("Init byte received: 0x%02X\n", byte);
            
            if (byte == 0x06) {
                // Send acknowledgment (ESC = 0x1B)
                serial.write(0x1B);
                serial.flush();  // Ensure it's sent
                Serial.println("MaCO2 start byte received, sent ACK (0x1B)");
                
                delay(50);  // Give sensor time to respond
                
                // Read and discard 7 initialization bytes
                int discarded = 0;
                unsigned long ackTime = millis();
                Serial.print("Reading init bytes: ");
                while (discarded < 7 && (millis() - ackTime < 2000)) {
                    if (serial.available()) {
                        uint8_t initByte = serial.read();
                        Serial.printf("0x%02X ", initByte);
                        discarded++;
                    }
                    delay(10);
                }
                Serial.println();
                
                if (discarded == 7) {
                    Serial.println("MaCO2 sensor initialized successfully");
                    _state = WAIT_FOR_DATA;
                    
                    // Flush any remaining bytes
                    delay(100);
                    while (serial.available()) {
                        serial.read();
                    }
                    
                    return true;
                } else {
                    Serial.printf("Failed to read initialization bytes (got %d/7)\n", discarded);
                    _errorCount++;
                    return false;
                }
            }
        }
        delay(100);
    }
    
    Serial.println("MaCO2 initialization timeout");
    _errorCount++;
    return false;
}

bool MaCO2Parser::parsePacket(HardwareSerial& serial, CO2Data& data) {
    // Process all available packets, but only return the most recent one
    bool gotPacket = false;
    int packetsProcessed = 0;
    const int MAX_PACKETS_PER_CALL = 10;  // Limit to prevent getting stuck

    while (packetsProcessed < MAX_PACKETS_PER_CALL && readPacket(serial)) {
        // Packet received successfully
        decodePacket(_rxBuffer, data);

        data.timestamp = millis();
        _lastPacketTime = data.timestamp;
        _packetCount++;
        gotPacket = true;
        packetsProcessed++;
    }

    // Log if we're processing multiple packets (indicates buffer buildup)
    if (packetsProcessed > 1) {
        Serial.printf("# Warning: Processed %d packets in one call (buffer catchup), %d bytes remaining\n",
                     packetsProcessed, serial.available());
    }

    return gotPacket;
}

bool MaCO2Parser::readPacket(HardwareSerial& serial) {
    // If we have too many consecutive errors, flush buffer and resync
    static int consecutiveErrors = 0;
    static uint8_t syncBuffer[16];  // Sliding window for sync search
    static int syncBufferLen = 0;
    static unsigned long syncStartTime = 0;

    unsigned long now = millis();

    // If we've been searching for sync for too long (>5 seconds), flush and restart
    if (consecutiveErrors > 3 && syncStartTime == 0) {
        syncStartTime = now;
    }

    if (syncStartTime > 0 && (now - syncStartTime) > 5000) {
        Serial.println("# Sync search timeout - flushing buffer and restarting");
        while (serial.available()) {
            serial.read();
        }
        consecutiveErrors = 0;
        syncBufferLen = 0;
        syncStartTime = 0;
        _state = WAIT_FOR_DATA;
        return false;
    }

    // Process bytes until we find a complete packet or run out of data
    while (serial.available()) {
        uint8_t byte = serial.read();
        
        switch (_state) {
            case WAIT_FOR_DATA:
                // Check if we need to search for sync using header
                if (consecutiveErrors > 3) {
                    // Only print sync message once when entering sync mode
                    static bool syncMessagePrinted = false;
                    if (!syncMessagePrinted) {
                        Serial.println("# === SYNC LOST - Searching using 0x06 header + checksum ===");
                        syncMessagePrinted = true;
                    }

                    // Collect bytes into sync buffer
                    if (syncBufferLen < 16) {
                        syncBuffer[syncBufferLen++] = byte;
                    }

                    // Once we have enough bytes, search for valid packet using header
                    if (syncBufferLen >= 10) {
                        // Try each position starting with 0x06 header
                        for (int offset = 0; offset <= syncBufferLen - 8; offset++) {
                            // Look for 0x06 header byte
                            if (syncBuffer[offset] != 0x06) {
                                continue;  // Not a header, skip
                            }
                            
                            // Check if we have full packet from this position
                            if (offset + 8 > syncBufferLen) {
                                break;  // Not enough bytes yet
                            }
                            
                            // Validate checksum
                            uint8_t sum = 0;
                            for (int i = 0; i < 7; i++) {
                                sum += syncBuffer[offset + i];
                            }
                            uint8_t expected_checksum = syncBuffer[offset + 7];
                            
                            // Validate RR - must be reasonable (0-60)
                            // Note: RR=0 is valid when sampling ambient air between test sessions
                            uint8_t rr_test = syncBuffer[offset + 2];

                            // Additional validation: check CO2 values are reasonable
                            uint8_t fco2_test = syncBuffer[offset + 4];  // d[4] - waveform (0-50 typical)
                            uint8_t fetco2_test = syncBuffer[offset + 5];  // d[5] - peak (0-120 typical)

                            bool co2_reasonable = (fco2_test <= 50 && fetco2_test <= 120);

                            if (sum == expected_checksum && rr_test <= 60 && co2_reasonable) {
                                // Found valid packet!
                                Serial.printf("# Found sync at offset %d: header=0x06, RR=%d, FCO2=%d, FetCO2=%d, checksum=0x%02X OK\n",
                                            offset, rr_test, fco2_test, fetco2_test, expected_checksum);

                                // Copy to rx buffer and process
                                memcpy(&_rxBuffer, &syncBuffer[offset], sizeof(MaCO2Packet));
                                consecutiveErrors = 0;
                                syncBufferLen = 0;
                                syncStartTime = 0;  // Reset sync timer
                                _state = WAIT_FOR_DATA;

                                // Reset sync message flag
                                static bool syncMessagePrinted = false;
                                syncMessagePrinted = false;

                                return true;
                            }
                        }
                        
                        // Shift buffer and continue searching
                        memmove(syncBuffer, syncBuffer + 4, 12);
                        syncBufferLen -= 4;
                    }
                    return false;
                }
                
                // Normal operation - look for 0x06 header to start packet
                if (byte == 0x06) {
                    syncBufferLen = 0;  // Reset sync buffer
                    ((uint8_t*)&_rxBuffer)[0] = byte;
                    _rxIndex = 1;
                    _state = READING_PACKET;
                }
                break;
                
            case READING_PACKET:
                ((uint8_t*)&_rxBuffer)[_rxIndex++] = byte;
                
                if (_rxIndex >= sizeof(MaCO2Packet)) {
                    // Full packet received - validate with checksum
                    uint8_t* bytes = (uint8_t*)&_rxBuffer;
                    uint8_t calculated_checksum = 0;
                    for (int i = 0; i < 7; i++) {
                        calculated_checksum += bytes[i];
                    }
                    
                    bool checksum_valid = (calculated_checksum == _rxBuffer.checksum);
                    bool header_valid = (_rxBuffer.status1 == 0x06);
                    bool rr_valid = (_rxBuffer.rr <= 60);  // RR can be 0-60 (0 is valid when sampling ambient air)

                    if (!checksum_valid) {
                        Serial.printf("# Checksum fail: calc=0x%02X got=0x%02X\n",
                                    calculated_checksum, _rxBuffer.checksum);
                        consecutiveErrors++;
                        _state = WAIT_FOR_DATA;
                        _rxIndex = 0;
                        _errorCount++;
                        break;
                    }

                    if (!header_valid) {
                        Serial.printf("# Header fail: d[0]=0x%02X (expected 0x06)\n", _rxBuffer.status1);
                        consecutiveErrors++;
                        _state = WAIT_FOR_DATA;
                        _rxIndex = 0;
                        _errorCount++;
                        break;
                    }

                    if (!rr_valid) {
                        Serial.printf("# RR fail: %d (must be 0-60)\n", _rxBuffer.rr);
                        consecutiveErrors++;
                        _state = WAIT_FOR_DATA;
                        _rxIndex = 0;
                        _errorCount++;
                        break;
                    }

                    // Additional sanity check on CO2 values
                    if (_rxBuffer.fco2_wave > 50 || _rxBuffer.fetco2 > 120) {
                        Serial.printf("# CO2 values out of range: FCO2=%d, FetCO2=%d\n",
                                    _rxBuffer.fco2_wave, _rxBuffer.fetco2);
                        consecutiveErrors++;
                        _state = WAIT_FOR_DATA;
                        _rxIndex = 0;
                        _errorCount++;
                        break;
                    }
                    
                    // Valid packet!
                    consecutiveErrors = 0;
                    syncStartTime = 0;  // Reset sync timer
                    _state = WAIT_FOR_DATA;
                    _rxIndex = 0;
                    return true;
                }
                break;
        }
    }
    
    // Check for timeout (no data received for 2 seconds)
    if (_state == READING_PACKET && 
        _lastPacketTime > 0 && 
        (millis() - _lastPacketTime) > 2000) {
        Serial.println("MaCO2 packet timeout - resyncing");
        consecutiveErrors++;
        _state = WAIT_FOR_DATA;
        _rxIndex = 0;
        _errorCount++;
    }
    
    return false;
}

void MaCO2Parser::decodePacket(const MaCO2Packet& packet, CO2Data& data) {
    uint8_t* bytes = (uint8_t*)&packet;

    // Validate checksum (sum of bytes 0-6, modulo 256)
    uint8_t calculated_checksum = 0;
    for (int i = 0; i < 7; i++) {
        calculated_checksum += bytes[i];
    }
    bool checksum_valid = (calculated_checksum == packet.checksum);

    // Validate packet
    if (!checksum_valid) {
        Serial.printf("# Checksum error: calc=0x%02X got=0x%02X\n",
                     calculated_checksum, packet.checksum);
        _errorCount++;
        data.valid = false;
        return;
    }

    if (packet.rr > 60) {  // RR > 60 is physiologically impossible
        Serial.printf("# Packet sync error: RR=%d (resetting)\n", packet.rr);
        _state = WAIT_FOR_DATA;
        _rxIndex = 0;
        _errorCount++;
        data.valid = false;
        return;
    }
    
    // Copy raw data from packet - CORRECTED MAPPING
    data.status1 = packet.status1;           // d[0] - Data valid flag (6 = valid)
    data.status2 = packet.status2;           // d[1] - Status
    data.respiratory_rate = packet.rr;       // d[2] - RR
    
    // CO2 values - CORRECTED based on data analysis
    data.fco2 = packet.fico2;                // d[3] - Inspired baseline (FiCO2)  
    data.co2_waveform = packet.fco2_wave;    // d[4] - 8Hz waveform curve (FCO2)
    data.fetco2 = packet.fetco2;             // d[5] - End-tidal peak (FetCO2)
    
    // Check data validity (d[0] should be 6 for valid data)
    data.valid = (packet.status1 == 6) && checksum_valid && isDataValid(data);
    
    // Note: ADC values (o2_adc, vol_adc, o2_percent, volume_ml) 
    // are filled in by ADCManager, not here
}

void MaCO2Parser::sendCommand(HardwareSerial& serial, MaCO2Command cmd) {
    serial.write((uint8_t)cmd);
    Serial.printf("Sent MaCO2 command: 0x%02X\n", cmd);
}

bool MaCO2Parser::isPumpRunning(const CO2Data& data) const {
    // Pump status bit: 0 = running (OK), 1 = stopped (problem)
    return (data.status2 & 0x01) == 0;
}

bool MaCO2Parser::isLeakDetected(const CO2Data& data) const {
    return (data.status2 & 0x02) != 0;
}

bool MaCO2Parser::isOcclusionDetected(const CO2Data& data) const {
    return (data.status2 & 0x04) != 0;
}

bool MaCO2Parser::isDataValid(const CO2Data& data) const {
    // Basic validity checks
    // Check if we're getting reasonable values
    if (data.respiratory_rate > 60) return false;  // RR > 60 is unlikely
    if (data.fetco2 > 150) return false;           // FetCO2 > 150 mmHg is extreme
    
    // Could add more sophisticated checks based on status1 flags
    // For now, assume data is valid if basic sanity checks pass
    return true;
}

void MaCO2Parser::resetStatistics() {
    _packetCount = 0;
    _errorCount = 0;
    _lastPacketTime = 0;
}
