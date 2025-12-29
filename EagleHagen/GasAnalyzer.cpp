#include "GasAnalyzer.h"

GasAnalyzer::GasAnalyzer() : o2(0), co2(0), volume(0), parsing(false) {}

void GasAnalyzer::processSerial() {
    while (Serial.available()) {
        char incoming = Serial.read();
        
        if (incoming == 27) { // ESC character detected
            parsing = true;
            buffer = ""; // Reset the buffer
        } else if (parsing) {
            if (incoming == '\n') { // End of data package
                parseData();
                parsing = false;
            } else {
                buffer += incoming; // Accumulate ASCII data
            }
        }
    }
}

void GasAnalyzer::parseData() {
    // Split buffer into three parts for O2, CO2, and volume
    int firstComma = buffer.indexOf(',');
    int secondComma = buffer.indexOf(',', firstComma + 1);

    if (firstComma != -1 && secondComma != -1) {
        o2 = buffer.substring(0, firstComma).toFloat();
        co2 = buffer.substring(firstComma + 1, secondComma).toFloat();
        volume = buffer.substring(secondComma + 1).toFloat();
    }
}

float GasAnalyzer::getO2() const {
    return o2;
}

float GasAnalyzer::getCO2() const {
    return co2;
}

float GasAnalyzer::getVolume() const {
    return volume;
}
