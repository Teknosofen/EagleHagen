#include "AccessPoint.h"

AccessPoint myAP("ESP8266_AP");
#include "GasAnalyzer.h"

GasAnalyzer gasAnalyzer;

void setup() {
    myAP.begin();
    Serial.begin(9600); // Start serial communication
}

void loop() {
    gasAnalyzer.processSerial(); // Process incoming data

    // Print measurements to the Serial Monitor
    Serial.print("O2: ");
    Serial.println(gasAnalyzer.getO2());
    Serial.print("CO2: ");
    Serial.println(gasAnalyzer.getCO2());
    Serial.print("Volume: ");
    Serial.println(gasAnalyzer.getVolume());
    
    myAP.updateNumbers(42.0, 84.0); // Update numbers displayed
    myAP.updateGraph(10, 20, 30, 40); // Add data points to the graph
    
    delay(1000); // Simulate periodic updates
}
