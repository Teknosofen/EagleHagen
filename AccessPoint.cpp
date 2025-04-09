#include "AccessPoint.h"

AccessPoint::AccessPoint(const char* ssidName) : ssid(ssidName), server(80), number1(0), number2(0) {}

void AccessPoint::begin() {
    WiFi.softAP(ssid); // Start open WiFi Access Point
    server.on("/", std::bind(&AccessPoint::handleRoot, this));
    server.begin(); // Start the server
}

void AccessPoint::handleRoot() {
    String page = "<!DOCTYPE html><html><body>";
    page += "<h1>ESP8266 Dashboard</h1>";
    page += "<p>Number 1: <span id=\"num1\">" + String(number1) + "</span></p>";
    page += "<p>Number 2: <span id=\"num2\">" + String(number2) + "</span></p>";
    page += "<canvas id=\"graph\" width=\"400\" height=\"200\" style=\"border:1px solid #000;\"></canvas>";
    page += "<script>";
    page += "const x1 = [" + graphDataX1 + "];"; // Curve 1 X values
    page += "const y1 = [" + graphDataY1 + "];"; // Curve 1 Y values
    page += "const x2 = [" + graphDataX2 + "];"; // Curve 2 X values
    page += "const y2 = [" + graphDataY2 + "];"; // Curve 2 Y values
    page += "const canvas = document.getElementById('graph');";
    page += "const ctx = canvas.getContext('2d');";
    page += "function drawGraph() {";
    page += "  ctx.clearRect(0, 0, canvas.width, canvas.height);";
    page += "  ctx.beginPath();";
    page += "  ctx.strokeStyle = 'blue';";
    page += "  for (let i = 0; i < x1.length; i++) {";
    page += "    ctx.lineTo(x1[i], y1[i]);";
    page += "  }";
    page += "  ctx.stroke();";
    page += "  ctx.beginPath();";
    page += "  ctx.strokeStyle = 'red';";
    page += "  for (let i = 0; i < x2.length; i++) {";
    page += "    ctx.lineTo(x2[i], y2[i]);";
    page += "  }";
    page += "  ctx.stroke();";
    page += "}";
    page += "drawGraph();"; // Draw on page load
    page += "</script></body></html>";

    server.send(200, "text/html", page);
}

void AccessPoint::updateNumbers(float num1, float num2) {
    number1 = num1;
    number2 = num2;
}

void AccessPoint::updateGraph(float x1, float y1, float x2, float y2) {
    graphDataX1 += String(x1) + ",";
    graphDataY1 += String(y1) + ",";
    graphDataX2 += String(x2) + ",";
    graphDataY2 += String(y2) + ",";
}
