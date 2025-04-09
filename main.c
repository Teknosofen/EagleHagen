#include "AccessPoint.h"

AccessPoint myAP("ESP8266_AP");

void setup() {
    myAP.begin();
}

void loop() {
    myAP.updateNumbers(42.0, 84.0); // Update numbers displayed
    myAP.updateGraph(10, 20, 30, 40); // Add data points to the graph
    delay(1000); // Simulate periodic updates
}
