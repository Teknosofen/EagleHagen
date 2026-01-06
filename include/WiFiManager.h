// WiFiManager.h
// Manages WiFi connection and web server
// Provides REST API and WebSocket interface for monitoring

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <ArduinoJson.h>
#include "MaCO2Parser.h"  // For CO2Data structure
#include "DataLogger.h"   // For output format control

class WiFiManager {
public:
    WiFiManager(uint16_t port = 80);
    ~WiFiManager();
    
    // Initialize WiFi as Access Point
    bool beginAP(const char* ssid, const char* password = nullptr);
    
    // Initialize WiFi as Station (connect to existing network)
    bool beginStation(const char* ssid, const char* password, 
                     unsigned long timeout_ms = 10000);
    
    // Start web server
    bool startServer();
    
    // Stop web server
    void stopServer();
    
    // Set DataLogger reference (for format control)
    void setDataLogger(DataLogger* logger) { _dataLogger = logger; }
    
    // Update with new data (broadcasts to WebSocket clients)
    void update(const CO2Data& data);
    
    // Handle WebSocket messages and execute commands
    void loop();
    
    // Check if client is connected
    bool hasClients() const;
    uint16_t getClientCount() const;
    
    // Get IP address
    IPAddress getIP() const;
    
    // Check if command is pending from web interface
    bool hasCommand();
    uint8_t getCommand();
    
private:
    AsyncWebServer* _server;
    AsyncWebSocket* _ws;
    
    uint16_t _port;
    bool _isAP;
    bool _serverRunning;
    
    // Command queue (simple FIFO)
    static const uint8_t CMD_QUEUE_SIZE = 10;
    uint8_t _cmdQueue[CMD_QUEUE_SIZE];
    uint8_t _cmdQueueHead;
    uint8_t _cmdQueueTail;
    
    // DataLogger reference (for format control)
    DataLogger* _dataLogger;
    
    // Web server handlers
    void handleRoot(AsyncWebServerRequest* request);
    void handleData(AsyncWebServerRequest* request);
    void handleCommand(AsyncWebServerRequest* request);
    void handleSetFormat(AsyncWebServerRequest* request);
    void handleNotFound(AsyncWebServerRequest* request);
    
    // WebSocket handlers
    void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                         AwsEventType type, void* arg, uint8_t* data, size_t len);
    
    // JSON serialization
    String dataToJson(const CO2Data& data);
    
    // Command queue management
    void enqueueCommand(uint8_t cmd);
    
    // Serve embedded web files (stored in PROGMEM)
    String getIndexHTML();
    String getStyleCSS();
    String getAppJS();
};

#endif // WIFI_MANAGER_H
