#include <Arduino.h>

class GasAnalyzer {
private:
    float o2;       // Oxygen measurement
    float co2;      // Carbon dioxide measurement
    float volume;   // Volume measurement
    bool parsing;   // Parsing flag
    String buffer;  // Temporary buffer for incoming data

    void parseData(); // Parses the buffer into measurements

public:
    GasAnalyzer();
    void processSerial(); // Called in loop to manage serial data
    float getO2() const;  // Access the O2 measurement
    float getCO2() const; // Access the CO2 measurement
    float getVolume() const; // Access the volume measurement
};
