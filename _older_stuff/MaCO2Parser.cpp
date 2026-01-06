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
    
    unsigned long startTime = millis();
    
    // Wait for start byte (0x06) from MaCO2
    while (millis() - startTime < timeout_ms) {
        if (serial.available()) {
            uint8_t byte = serial.read();
            if (byte == 0x06) {
                // Send acknowledgment (ESC = 0x1B)
                serial.write(0x1B);
                Serial.println("MaCO2 start byte received, sent ACK");
                
                // Read and discard 7 initialization bytes
                int discarded = 0;
                unsigned long ackTime = millis();
                while (discarded < 7 && (millis() - ackTime < 2000)) {
                    if (serial.available()) {
                        serial.read();
                        discarded++;
                    }
                    delay(10);
                }
                
                if (discarded == 7) {
                    Serial.println("MaCO2 sensor initialized successfully");
                    _state = WAIT_FOR_DATA;
                    return true;
                } else {
                    Serial.println("Failed to read initialization bytes");
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
    if (!readPacket(serial)) {
        return false;
    }
    
    // Packet received successfully
    decodePacket(_rxBuffer, data);
    
    data.timestamp = millis();
    _lastPacketTime = data.timestamp;
    _packetCount++;
    
    return true;
}

bool MaCO2Parser::readPacket(HardwareSerial& serial) {
    while (serial.available()) {
        uint8_t byte = serial.read();
        
        switch (_state) {
            case WAIT_FOR_DATA:
                // Start receiving packet
                ((uint8_t*)&_rxBuffer)[0] = byte;
                _rxIndex = 1;
                _state = READING_PACKET;
                break;
                
            case READING_PACKET:
                ((uint8_t*)&_rxBuffer)[_rxIndex++] = byte;
                
                if (_rxIndex >= sizeof(MaCO2Packet)) {
                    // Full packet received
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
        Serial.println("MaCO2 packet timeout");
        _state = WAIT_FOR_DATA;
        _rxIndex = 0;
        _errorCount++;
    }
    
    return false;
}

void MaCO2Parser::decodePacket(const MaCO2Packet& packet, CO2Data& data) {
    // Copy raw data from packet
    data.status1 = packet.status1;
    data.status2 = packet.status2;
    data.respiratory_rate = packet.rr;
    data.fetco2 = packet.fetco2;
    data.fco2 = packet.fico2;  // Note: packet field is named fico2 but we call it fco2
    data.co2_waveform = packet.fetco2;  // Waveform is the FetCO2 value
    
    // Check data validity
    data.valid = isDataValid(data);
    
    // Note: ADC values (o2_adc, vol_adc, o2_percent, volume_ml) 
    // are filled in by ADCManager, not here
}

void MaCO2Parser::sendCommand(HardwareSerial& serial, MaCO2Command cmd) {
    serial.write((uint8_t)cmd);
    Serial.printf("Sent MaCO2 command: 0x%02X\n", cmd);
}

bool MaCO2Parser::isPumpRunning(const CO2Data& data) const {
    return (data.status2 & 0x01) != 0;
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
