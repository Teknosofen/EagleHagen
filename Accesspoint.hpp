#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

class AccessPoint {
private:
    const char* ssid; // SSID of the AP
    ESP8266WebServer server; // Web server
    float number1, number2; // Data to display
    String graphDataX1, graphDataY1, graphDataX2, graphDataY2; // Data for two curves

    void handleRoot();

public:
    AccessPoint(const char* ssidName);
    void begin();
    void updateNumbers(float num1, float num2);
    void updateGraph(float x1, float y1, float x2, float y2);
};
