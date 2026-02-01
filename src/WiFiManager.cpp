// WiFiManager.cpp
// Implementation of WiFi and web server management

#include "WiFiManager.h"
#include "ChartJS.h"

// Store pointer for static callback
static WiFiManager* _instance = nullptr;

WiFiManager::WiFiManager(uint16_t port)
    : _server(nullptr)
    , _ws(nullptr)
    , _port(port)
    , _isAP(false)
    , _serverRunning(false)
    , _cmdQueueHead(0)
    , _cmdQueueTail(0)
    , _dataLogger(nullptr)
{
    _instance = this;
    memset(_cmdQueue, 0, sizeof(_cmdQueue));
}

WiFiManager::~WiFiManager() {
    stopServer();
    if (_server) delete _server;
    if (_ws) delete _ws;
}

bool WiFiManager::beginAP(const char* ssid, const char* password) {
    Serial.println("Starting WiFi Access Point...");
    
    _isAP = true;
    
    // Start AP with or without password
    bool success;
    if (password && strlen(password) > 0) {
        success = WiFi.softAP(ssid, password);
    } else {
        success = WiFi.softAP(ssid);
    }
    
    if (!success) {
        Serial.println("Failed to start AP");
        return false;
    }
    
    IPAddress ip = WiFi.softAPIP();
    Serial.printf("AP Started: SSID='%s'\n", ssid);
    Serial.printf("IP Address: %s\n", ip.toString().c_str());
    
    return true;
}

bool WiFiManager::beginStation(const char* ssid, const char* password, unsigned long timeout_ms) {
    Serial.printf("Connecting to WiFi: %s\n", ssid);
    
    _isAP = false;
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < timeout_ms) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Failed to connect to WiFi");
        return false;
    }
    
    Serial.printf("Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    return true;
}

bool WiFiManager::startServer() {
    if (_serverRunning) {
        Serial.println("Server already running");
        return true;
    }
    
    // Create server and WebSocket
    _server = new AsyncWebServer(_port);
    _ws = new AsyncWebSocket("/ws");
    
    // Attach WebSocket to server
    _ws->onEvent([](AsyncWebSocket* server, AsyncWebSocketClient* client,
                   AwsEventType type, void* arg, uint8_t* data, size_t len) {
        if (_instance) {
            _instance->onWebSocketEvent(server, client, type, arg, data, len);
        }
    });
    _server->addHandler(_ws);
    
    // Setup routes
    _server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleRoot(request);
    });
    
    _server->on("/data", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleData(request);
    });
    
    _server->on("/command", HTTP_POST, [this](AsyncWebServerRequest* request) {
        handleCommand(request);
    });
    
    _server->on("/api/setFormat", HTTP_GET, [this](AsyncWebServerRequest* request) {
        handleSetFormat(request);
    });

    // Serve Chart.js from embedded gzip data (no internet required)
    _server->on("/chart.min.js", HTTP_GET, [](AsyncWebServerRequest* request) {
        AsyncWebServerResponse* response = request->beginResponse(200, "application/javascript", CHARTJS_GZ, CHARTJS_GZ_LEN);
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "public, max-age=31536000");
        request->send(response);
    });

    _server->onNotFound([this](AsyncWebServerRequest* request) {
        handleNotFound(request);
    });
    
    // Start server
    _server->begin();
    _serverRunning = true;
    
    Serial.println("Web server started");
    return true;
}

void WiFiManager::stopServer() {
    if (_server && _serverRunning) {
        _server->end();
        _serverRunning = false;
        Serial.println("Web server stopped");
    }
}

void WiFiManager::update(const CO2Data& data) {
    if (!_serverRunning) return;
    
    // Broadcast to all WebSocket clients
    String json = dataToJson(data);
    _ws->textAll(json);
}

void WiFiManager::loop() {
    if (_ws) {
        _ws->cleanupClients();
    }
}

bool WiFiManager::hasClients() const {
    return _ws && _ws->count() > 0;
}

uint16_t WiFiManager::getClientCount() const {
    return _ws ? _ws->count() : 0;
}

IPAddress WiFiManager::getIP() const {
    return _isAP ? WiFi.softAPIP() : WiFi.localIP();
}

bool WiFiManager::hasCommand() {
    return _cmdQueueHead != _cmdQueueTail;
}

uint8_t WiFiManager::getCommand() {
    if (!hasCommand()) return 0;
    
    uint8_t cmd = _cmdQueue[_cmdQueueTail];
    _cmdQueueTail = (_cmdQueueTail + 1) % CMD_QUEUE_SIZE;
    return cmd;
}

void WiFiManager::handleRoot(AsyncWebServerRequest* request) {
    request->send(200, "text/html", getIndexHTML());
}

void WiFiManager::handleData(AsyncWebServerRequest* request) {
    // Return last known data (stored in update())
    // For now, return empty - real data comes via WebSocket
    request->send(200, "application/json", "{\"status\":\"use websocket\"}");
}

void WiFiManager::handleCommand(AsyncWebServerRequest* request) {
    if (!request->hasParam("cmd", true)) {
        request->send(400, "text/plain", "Missing 'cmd' parameter");
        return;
    }
    
    String cmdStr = request->getParam("cmd", true)->value();
    uint8_t cmd = 0;
    
    if (cmdStr == "start_pump") {
        cmd = 0xA5;
    } else if (cmdStr == "zero_cal") {
        cmd = 0x5A;
    } else {
        request->send(400, "text/plain", "Invalid command");
        return;
    }
    
    enqueueCommand(cmd);
    request->send(200, "text/plain", "OK");
}

void WiFiManager::handleSetFormat(AsyncWebServerRequest* request) {
    if (!request->hasParam("format")) {
        request->send(400, "text/plain", "Missing 'format' parameter");
        return;
    }
    
    int format = request->getParam("format")->value().toInt();
    
    if (format < 0 || format > 1) {
        request->send(400, "text/plain", "Invalid format (0=Legacy, 1=TabSep)");
        return;
    }
    
    // Set format via external reference (passed to WiFiManager)
    if (_dataLogger) {
        _dataLogger->setOutputFormat(format == 0 ? FORMAT_LEGACY_LABVIEW : FORMAT_TAB_SEPARATED);
        Serial.printf("Output format changed to: %s\n", 
                     format == 0 ? "Legacy LabVIEW" : "Tab-Separated ASCII");
        request->send(200, "text/plain", "OK");
    } else {
        request->send(500, "text/plain", "DataLogger not available");
    }
}

void WiFiManager::handleNotFound(AsyncWebServerRequest* request) {
    request->send(404, "text/plain", "Not found");
}

void WiFiManager::onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                                   AwsEventType type, void* arg, uint8_t* data, size_t len) {
    switch (type) {
        case WS_EVT_CONNECT:
            Serial.printf("WebSocket client #%u connected from %s\n", 
                         client->id(), client->remoteIP().toString().c_str());
            break;
            
        case WS_EVT_DISCONNECT:
            Serial.printf("WebSocket client #%u disconnected\n", client->id());
            break;
            
        case WS_EVT_DATA: {
            // Handle incoming messages (commands from web interface)
            AwsFrameInfo* info = (AwsFrameInfo*)arg;
            if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                data[len] = 0;  // Null terminate
                String msg = (char*)data;
                
                // Parse JSON command
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, msg);

                if (!error && doc["cmd"].is<const char*>()) {
                    String cmd = doc["cmd"];
                    if (cmd == "start_pump") {
                        enqueueCommand(0xA5);
                    } else if (cmd == "zero_cal") {
                        enqueueCommand(0x5A);
                    }
                }
            }
            break;
        }
        
        default:
            break;
    }
}

String WiFiManager::dataToJson(const CO2Data& data) {
    JsonDocument doc;
    
    doc["timestamp"] = data.timestamp;
    doc["co2_waveform"] = data.co2_waveform;
    doc["fetco2"] = data.fetco2;
    doc["fco2"] = data.fco2;
    doc["rr"] = data.respiratory_rate;
    doc["o2_percent"] = data.o2_percent;
    doc["volume_ml"] = data.volume_ml;
    doc["status1"] = data.status1;
    doc["status2"] = data.status2;
    doc["valid"] = data.valid;
    
    // Status flags (bit=0 means OK for pump, bit=1 means problem for leak/occlusion)
    doc["pump_running"] = (data.status2 & 0x01) == 0;  // 0=running, 1=stopped
    doc["leak_detected"] = (data.status2 & 0x02) != 0;
    doc["occlusion_detected"] = (data.status2 & 0x04) != 0;
    
    String output;
    serializeJson(doc, output);
    return output;
}

void WiFiManager::enqueueCommand(uint8_t cmd) {
    uint8_t nextHead = (_cmdQueueHead + 1) % CMD_QUEUE_SIZE;
    if (nextHead != _cmdQueueTail) {
        _cmdQueue[_cmdQueueHead] = cmd;
        _cmdQueueHead = nextHead;
        Serial.printf("Command enqueued: 0x%02X\n", cmd);
    } else {
        Serial.println("Command queue full!");
    }
}

// HTML page with embedded CSS and JavaScript
String WiFiManager::getIndexHTML() {
    return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Örnhagens Monitor</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: #84B6D6;
            min-height: 100vh;
            padding: 15px;
        }

        .container {
            max-width: 1200px;
            margin: 0 auto;
            background: #f0f4f8;
            border-radius: 12px;
            box-shadow: 0 10px 40px rgba(0,0,0,0.2);
            padding: 15px 20px;
        }

        .header-row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 12px;
            flex-wrap: wrap;
            gap: 8px;
        }

        h1 {
            color: #184D7B;
            font-size: 1.4em;
            margin: 0;
            display: flex;
            align-items: center;
        }

        .header-right {
            display: flex;
            align-items: center;
            gap: 10px;
        }

        .status-badge {
            padding: 2px 6px;
            border-radius: 8px;
            font-size: 0.65em;
            font-weight: bold;
            white-space: nowrap;
        }

        .status-badge.connected {
            background: #5AB66B;
            color: white;
        }

        .status-badge.disconnected {
            background: #e57373;
            color: white;
        }

        .status-badge.status-ok {
            background: #5AB66B;
            color: white;
        }

        .status-badge.status-error {
            background: #e57373;
            color: white;
            animation: pulse 2s infinite;
        }

        .byline {
            font-size: 0.7em;
            color: #5292BD;
            font-style: italic;
        }

        .metrics-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
            gap: 10px;
            margin-bottom: 15px;
        }

        .metric-card {
            background: linear-gradient(135deg, #5292BD 0%, #3951BD 100%);
            color: white;
            padding: 8px 10px;
            border-radius: 8px;
            box-shadow: 0 3px 6px rgba(0,0,0,0.15);
            text-align: center;
        }

        .metric-label {
            font-size: 0.7em;
            opacity: 0.95;
            margin-bottom: 2px;
        }

        .metric-value {
            font-size: 1.5em;
            font-weight: bold;
        }

        .metric-unit {
            display: none;
        }
        
        .chart-container {
            background: #f8f9fa;
            border-radius: 8px;
            padding: 12px 15px;
            margin-bottom: 12px;
        }

        .chart-title {
            font-size: 1em;
            color: #184D7B;
            margin-bottom: 8px;
            font-weight: 600;
        }

        canvas {
            width: 100% !important;
            height: auto !important;
            max-height: 200px;
        }
        
        @keyframes pulse {
            0%, 100% { opacity: 1; }
            50% { opacity: 0.7; }
        }

        .controls {
            display: flex;
            gap: 10px;
            justify-content: center;
            flex-wrap: wrap;
        }

        button {
            padding: 8px 16px;
            font-size: 0.85em;
            font-weight: 600;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            transition: all 0.2s;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }

        .btn-primary {
            background: #184D7B;
            color: white;
        }

        .btn-primary:hover {
            background: #29697B;
            box-shadow: 0 4px 8px rgba(0,0,0,0.2);
        }

        .btn-primary:active {
            transform: scale(0.98);
        }

        .btn-secondary {
            background: #5292BD;
            color: white;
        }

        .btn-secondary:hover {
            background: #3951BD;
            box-shadow: 0 4px 8px rgba(0,0,0,0.2);
        }
        
        @media (max-width: 768px) {
            .container {
                padding: 15px;
            }
            
            h1 {
                font-size: 1.5em;
            }
            
            .metric-value {
                font-size: 2em;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header-row">
            <h1>
                <img src="data:image/webp;base64,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" alt="Örnhagen Logo" style="height: 40px; vertical-align: middle; margin-right: 15px;" />
                Örnhagens Monitor
            </h1>
            <div class="header-right">
                <span class="byline">by Teknosofen</span>
                <div id="connectionStatus" class="status-badge disconnected">Connecting...</div>
                <div id="pumpStatus" class="status-badge status-ok">Pump</div>
                <div id="leakStatus" class="status-badge status-ok">Leak</div>
                <div id="occlusionStatus" class="status-badge status-ok">Occl</div>
            </div>
        </div>

        <!-- Instant Values -->
        <div class="metrics-grid">
            <div class="metric-card">
                <div class="metric-label">End-Tidal CO₂ [kPa]</div>
                <div class="metric-value" id="fetco2">--</div>
            </div>

            <div class="metric-card">
                <div class="metric-label">CO₂ [kPa]</div>
                <div class="metric-value" id="fco2">--</div>
            </div>

            <div class="metric-card">
                <div class="metric-label">RR [bpm]</div>
                <div class="metric-value" id="rr">--</div>
            </div>

            <div class="metric-card">
                <div class="metric-label">O₂ [%]</div>
                <div class="metric-value" id="o2">--</div>
            </div>

            <div class="metric-card">
                <div class="metric-label">Volume [mL]</div>
                <div class="metric-value" id="volume">--</div>
            </div>
        </div>

        <!-- Charts -->
        <div class="chart-container">
            <div class="chart-title">CO₂ Waveform</div>
            <canvas id="co2Chart"></canvas>
        </div>

        <div class="chart-container">
            <div class="chart-title">O₂</div>
            <canvas id="o2Chart"></canvas>
        </div>

        <div class="chart-container">
            <div class="chart-title">Volume</div>
            <canvas id="volumeChart"></canvas>
        </div>

        <!-- Controls -->
        <div class="controls">
            <button class="btn-primary" onclick="sendCommand('start_pump')">
                Start Pump
            </button>
            <button class="btn-secondary" onclick="sendCommand('zero_cal')">
                Zero Calibration
            </button>
            <button class="btn-primary" onclick="saveDataCSV()">
                Save Data (CSV)
            </button>
            <button class="btn-primary" onclick="saveDataJSON()">
                Save Data (JSON)
            </button>
            <button class="btn-secondary" onclick="clearData()">
                Clear Data
            </button>
        </div>
        
        <!-- Output Format Selection -->
        <div style="margin: 12px auto; max-width: 550px; padding: 10px; background: #f5f5f5; border-radius: 6px;">
            <div style="font-weight: bold; margin-bottom: 8px; color: #184D7B; font-size: 0.85em;">Host Output Format (8Hz @ 115200 baud)</div>
            <div style="display: flex; gap: 10px; align-items: center;">
                <label style="flex: 1; cursor: pointer; padding: 8px; background: white; border: 2px solid #ddd; border-radius: 4px; font-size: 0.85em;">
                    <input type="radio" name="outputFormat" value="legacy" onchange="setOutputFormat(0)">
                    <strong>Legacy (LabVIEW)</strong><br>
                    <small style="color: #666;">Binary format with ADC values</small>
                </label>
                <label style="flex: 1; cursor: pointer; padding: 8px; background: white; border: 2px solid #ddd; border-radius: 4px; font-size: 0.85em;">
                    <input type="radio" name="outputFormat" value="tabsep" checked onchange="setOutputFormat(1)">
                    <strong>Tab-Separated ASCII</strong><br>
                    <small style="color: #666;">Status, RR, FCO2, FetCO2</small>
                </label>
            </div>
        </div>

        <div style="text-align: center; margin-top: 12px; color: #5292BD; font-size: 0.8em;">
            <div>Data Points: <span id="dataCount">0</span> / 960 (2 min buffer) | Duration: <span id="duration">0:00</span></div>
        </div>
    </div>
    
    <script src="/chart.min.js"></script>
    <script>
        // WebSocket connection
        let ws;
        let reconnectInterval;
        
        // Chart configuration
        const maxDataPoints = 960; // 2 minutes at 8 Hz (8 * 60 * 2)
        
        // Chart.js instances
        let co2Chart, o2Chart, volumeChart;
        
        // Data buffers
        let co2Data = [];
        let o2Data = [];
        let volumeData = [];
        let timeLabels = [];
        
        // Complete data storage for export (includes all received data)
        let dataLog = [];
        let recordingStartTime = null;
        
        function initWebSocket() {
            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsUrl = `${protocol}//${window.location.host}/ws`;
            
            ws = new WebSocket(wsUrl);
            
            ws.onopen = function() {
                console.log('WebSocket connected');
                updateConnectionStatus(true);
                if (reconnectInterval) {
                    clearInterval(reconnectInterval);
                    reconnectInterval = null;
                }
            };
            
            ws.onclose = function() {
                console.log('WebSocket disconnected');
                updateConnectionStatus(false);
                // Attempt to reconnect every 3 seconds
                if (!reconnectInterval) {
                    reconnectInterval = setInterval(initWebSocket, 3000);
                }
            };
            
            ws.onerror = function(error) {
                console.error('WebSocket error:', error);
            };
            
            ws.onmessage = function(event) {
                try {
                    const data = JSON.parse(event.data);
                    updateDisplay(data);
                } catch (e) {
                    console.error('Error parsing data:', e);
                }
            };
        }
        
        function updateConnectionStatus(connected) {
            const statusDiv = document.getElementById('connectionStatus');
            if (connected) {
                statusDiv.className = 'status-badge connected';
                statusDiv.textContent = 'Connected';
            } else {
                statusDiv.className = 'status-badge disconnected';
                statusDiv.textContent = 'Reconnecting...';
            }
        }
        
        function updateDisplay(data) {
            // Update instant values
            document.getElementById('fetco2').textContent = ((data.fetco2 || 0) * 0.133322).toFixed(1);
            document.getElementById('fco2').textContent = ((data.co2_waveform || 0) * 0.133322).toFixed(1);
            document.getElementById('rr').textContent = data.rr || '--';
            document.getElementById('o2').textContent = data.o2_percent ? data.o2_percent.toFixed(1) : '--';
            document.getElementById('volume').textContent = data.volume_ml ? Math.round(data.volume_ml) : '--';
            
            // Update status indicators
            updateStatus('pumpStatus', data.pump_running, 'Pump', 'Pump!');
            updateStatus('leakStatus', !data.leak_detected, 'Leak', 'Leak!');
            updateStatus('occlusionStatus', !data.occlusion_detected, 'Occl', 'Occl!');
            
            // Get current time
            const now = new Date();
            const timeStr = now.toLocaleTimeString();
            
            // Store complete data for export
            if (!recordingStartTime) {
                recordingStartTime = now;
            }
            
            dataLog.push({
                timestamp: now.toISOString(),
                elapsed_seconds: (now - recordingStartTime) / 1000,
                co2_waveform: (data.co2_waveform || 0) * 0.133322,
                fetco2: (data.fetco2 || 0) * 0.133322,
                fco2: (data.fco2 || 0) * 0.133322,
                rr: data.rr || 0,
                o2_percent: data.o2_percent || 0,
                volume_ml: data.volume_ml || 0,
                pump_running: data.pump_running || false,
                leak_detected: data.leak_detected || false,
                occlusion_detected: data.occlusion_detected || false,
                status1: data.status1 || 0,
                status2: data.status2 || 0
            });
            
            // Update chart data
            co2Data.push((data.co2_waveform || 0) * 0.133322);  // Convert mmHg to kPa
            o2Data.push(data.o2_percent || 0);
            volumeData.push(data.volume_ml || 0);
            timeLabels.push(timeStr);
            
            // Keep only last maxDataPoints in charts
            if (co2Data.length > maxDataPoints) {
                co2Data.shift();
                o2Data.shift();
                volumeData.shift();
                timeLabels.shift();
            }
            
            // Keep only last maxDataPoints in dataLog
            if (dataLog.length > maxDataPoints) {
                dataLog.shift();
            }
            
            // Update counters
            document.getElementById('dataCount').textContent = dataLog.length;
            
            const elapsed = (now - recordingStartTime) / 1000;
            const minutes = Math.floor(elapsed / 60);
            const seconds = Math.floor(elapsed % 60);
            document.getElementById('duration').textContent = `${minutes}:${seconds.toString().padStart(2, '0')}`;
            
            updateCharts();
        }
        
        function updateStatus(elementId, isOk, okText, errorText) {
            const element = document.getElementById(elementId);
            if (isOk) {
                element.className = 'status-badge status-ok';
                element.textContent = okText;
            } else {
                element.className = 'status-badge status-error';
                element.textContent = errorText;
            }
        }
        
        function updateCharts() {
            co2Chart.data.labels = timeLabels;
            co2Chart.data.datasets[0].data = co2Data;
            co2Chart.update('none'); // Update without animation for performance
            
            o2Chart.data.labels = timeLabels;
            o2Chart.data.datasets[0].data = o2Data;
            o2Chart.update('none');
            
            volumeChart.data.labels = timeLabels;
            volumeChart.data.datasets[0].data = volumeData;
            volumeChart.update('none');
        }
        
        function sendCommand(cmd) {
            if (ws && ws.readyState === WebSocket.OPEN) {
                const message = JSON.stringify({ cmd: cmd });
                ws.send(message);
                console.log('Command sent:', cmd);
            } else {
                alert('Not connected to sensor');
            }
        }
        
        function setOutputFormat(format) {
            fetch('/api/setFormat?format=' + format)
                .then(response => response.text())
                .then(data => {
                    console.log('Output format changed:', format === 0 ? 'Legacy LabVIEW' : 'Tab-Separated');
                })
                .catch(error => console.error('Error setting format:', error));
        }
        
        function initCharts() {
            const chartConfig = {
                type: 'line',
                options: {
                    responsive: true,
                    maintainAspectRatio: false,
                    animation: false,
                    scales: {
                        x: {
                            display: true,
                            title: {
                                display: true,
                                text: 'Time'
                            },
                            ticks: {
                                maxRotation: 45,
                                minRotation: 45,
                                autoSkip: true,
                                maxTicksLimit: 10
                            }
                        },
                        y: {
                            beginAtZero: true
                        }
                    },
                    plugins: {
                        legend: {
                            display: false
                        }
                    },
                    elements: {
                        line: {
                            tension: 0.4
                        },
                        point: {
                            radius: 0
                        }
                    }
                }
            };
            
            // CO2 Chart
            co2Chart = new Chart(document.getElementById('co2Chart'), {
                ...chartConfig,
                data: {
                    labels: timeLabels,
                    datasets: [{
                        data: co2Data,
                        borderColor: 'rgb(102, 126, 234)',
                        backgroundColor: 'rgba(102, 126, 234, 0.1)',
                        borderWidth: 2,
                        fill: true
                    }]
                },
                options: {
                    ...chartConfig.options,
                    scales: {
                        ...chartConfig.options.scales,
                        y: {
                            ...chartConfig.options.scales.y,
                            title: {
                                display: true,
                                text: 'CO₂ Waveform (kPa)'
                            },
                            suggestedMin: 0,
                            suggestedMax: 8
                        }
                    }
                }
            });
            
            // O2 Chart
            o2Chart = new Chart(document.getElementById('o2Chart'), {
                ...chartConfig,
                data: {
                    labels: timeLabels,
                    datasets: [{
                        data: o2Data,
                        borderColor: 'rgb(75, 192, 192)',
                        backgroundColor: 'rgba(75, 192, 192, 0.1)',
                        borderWidth: 2,
                        fill: true
                    }]
                },
                options: {
                    ...chartConfig.options,
                    scales: {
                        ...chartConfig.options.scales,
                        y: {
                            ...chartConfig.options.scales.y,
                            min: 0,
                            max: 100,
                            title: {
                                display: true,
                                text: 'O₂ (%)'
                            }
                        }
                    }
                }
            });
            
            // Volume Chart
            volumeChart = new Chart(document.getElementById('volumeChart'), {
                ...chartConfig,
                data: {
                    labels: timeLabels,
                    datasets: [{
                        data: volumeData,
                        borderColor: 'rgb(255, 99, 132)',
                        backgroundColor: 'rgba(255, 99, 132, 0.1)',
                        borderWidth: 2,
                        fill: true
                    }]
                },
                options: {
                    ...chartConfig.options,
                    scales: {
                        ...chartConfig.options.scales,
                        y: {
                            ...chartConfig.options.scales.y,
                            title: {
                                display: true,
                                text: 'Volume (mL)'
                            },
                            suggestedMin: 0,
                            suggestedMax: 1000
                        }
                    }
                }
            });
        }
        
        function saveDataCSV() {
            if (dataLog.length === 0) {
                alert('No data to save');
                return;
            }
            
            // Create CSV header
            let csv = 'Timestamp,Elapsed(s),CO2_Waveform(kPa),FetCO2(kPa),FiCO2(kPa),RR(bpm),O2(%),Volume(mL),Pump_Running,Leak_Detected,Occlusion_Detected,Status1,Status2\n';

            // Add data rows
            dataLog.forEach(row => {
                csv += `${row.timestamp},${row.elapsed_seconds.toFixed(3)},${row.co2_waveform.toFixed(2)},${row.fetco2.toFixed(2)},${row.fco2.toFixed(2)},${row.rr},${row.o2_percent.toFixed(2)},${row.volume_ml.toFixed(1)},${row.pump_running},${row.leak_detected},${row.occlusion_detected},${row.status1},${row.status2}\n`;
            });
            
            // Create download
            const blob = new Blob([csv], { type: 'text/csv' });
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `medair_co2_data_${new Date().toISOString().replace(/[:.]/g, '-')}.csv`;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            window.URL.revokeObjectURL(url);
            
            console.log(`Saved ${dataLog.length} data points to CSV`);
        }
        
        function saveDataJSON() {
            if (dataLog.length === 0) {
                alert('No data to save');
                return;
            }
            
            // Create JSON structure with metadata
            const exportData = {
                metadata: {
                    export_time: new Date().toISOString(),
                    recording_start: recordingStartTime ? recordingStartTime.toISOString() : null,
                    duration_seconds: recordingStartTime ? (new Date() - recordingStartTime) / 1000 : 0,
                    data_points: dataLog.length,
                    sample_rate_hz: 8,
                    device: 'MedAir CO2 Monitor',
                    firmware: 'ESP32-S3',
                    units: {
                        co2_waveform: 'kPa',
                        fetco2: 'kPa',
                        fco2: 'kPa',
                        rr: 'bpm',
                        o2_percent: '%',
                        volume_ml: 'mL'
                    }
                },
                data: dataLog
            };
            
            // Create download
            const json = JSON.stringify(exportData, null, 2);
            const blob = new Blob([json], { type: 'application/json' });
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = `medair_co2_data_${new Date().toISOString().replace(/[:.]/g, '-')}.json`;
            document.body.appendChild(a);
            a.click();
            document.body.removeChild(a);
            window.URL.revokeObjectURL(url);
            
            console.log(`Saved ${dataLog.length} data points to JSON`);
        }
        
        function clearData() {
            if (dataLog.length === 0) {
                alert('No data to clear');
                return;
            }
            
            if (!confirm(`Clear ${dataLog.length} data points? This cannot be undone.`)) {
                return;
            }
            
            // Clear all data
            dataLog = [];
            co2Data = [];
            o2Data = [];
            volumeData = [];
            timeLabels = [];
            recordingStartTime = null;
            
            // Update counters
            document.getElementById('dataCount').textContent = '0';
            document.getElementById('duration').textContent = '0:00';
            
            // Update charts
            updateCharts();
            
            console.log('Data cleared');
        }
        
        // Initialize on page load
        window.addEventListener('load', function() {
            initCharts();
            initWebSocket();
        });
    </script>
</body>
</html>
)rawliteral";
}

String WiFiManager::getStyleCSS() {
    // CSS is embedded in HTML
    return "";
}

String WiFiManager::getAppJS() {
    // JavaScript is embedded in HTML
    return "";
}
