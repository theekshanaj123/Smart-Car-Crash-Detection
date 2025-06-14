#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <math.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

// WiFi credentials
const char* ssid = "Dialog 4G 929";
const char* password = "60c302f3";

WebServer server(80);

#define BUTTON_PIN 4
bool stopAlert = false;

#define PIN 48
#define NUMPIXELS 1

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
#define DELAYVAL 500

// ========== MPU6050 Configuration ========== //
const int MPU_addr = 0x68;  // I2C address for MPU6050 (default: SDA=8, SCL=9 on ESP32-S3)

// Raw sensor data variables
int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ, Amp, gyroMagnitude;

// Processed sensor data (accelerometer in g's and gyroscope in °/s)
float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;

// Global variable for crash detection count (to filter out noise)
int crashCount = 0;

// Global variable to hold the time until which "CRASH DETECTED" is shown
unsigned long crashDisplayUntil = 0;

// Web interface HTML
const char* html_page = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Crash Detection Monitor</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body {
            font-family: Arial, sans-serif;
            margin: 0;
            padding: 20px;
            background-color: #1a1a1a;
            color: #ffffff;
        }
        .container {
            max-width: 1200px;
            margin: 0 auto;
        }
        .header {
            text-align: center;
            margin-bottom: 30px;
        }
        .status-panel {
            background: #2a2a2a;
            border-radius: 10px;
            padding: 20px;
            margin-bottom: 20px;
            border-left: 5px solid #00ff00;
        }
        .crash-alert {
            border-left-color: #ff0000 !important;
            background: #4a2a2a !important;
            animation: pulse 1s infinite;
        }
        @keyframes pulse {
            0% { opacity: 1; }
            50% { opacity: 0.7; }
            100% { opacity: 1; }
        }
        .data-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
            margin-bottom: 20px;
        }
        .data-card {
            background: #2a2a2a;
            border-radius: 10px;
            padding: 20px;
            border: 1px solid #444;
        }
        .data-value {
            font-size: 2em;
            font-weight: bold;
            color: #00ff00;
            margin: 10px 0;
        }
        .data-label {
            color: #aaa;
            font-size: 0.9em;
        }
        .chart-container {
            background: #2a2a2a;
            border-radius: 10px;
            padding: 20px;
            margin-bottom: 20px;
        }
        canvas {
            max-width: 100%;
            height: auto;
        }
        .button {
            background: #ff4444;
            color: white;
            border: none;
            padding: 15px 30px;
            border-radius: 5px;
            font-size: 1.1em;
            cursor: pointer;
            margin: 10px;
        }
        .button:hover {
            background: #ff6666;
        }
        .log-container {
            background: #1a1a1a;
            border: 1px solid #444;
            border-radius: 5px;
            padding: 15px;
            height: 200px;
            overflow-y: auto;
            font-family: monospace;
            font-size: 0.9em;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>Crash Detection Monitor</h1>
            <p>Real-time monitoring of acceleration and crash detection</p>
        </div>
        
        <div id="status-panel" class="status-panel">
            <h2 id="status-text">System Active</h2>
            <p id="status-desc">Monitoring for crashes...</p>
        </div>
        
        <div class="data-grid">
            <div class="data-card">
                <div class="data-label">Acceleration X</div>
                <div id="accel-x" class="data-value">0.00 g</div>
            </div>
            <div class="data-card">
                <div class="data-label">Acceleration Y</div>
                <div id="accel-y" class="data-value">0.00 g</div>
            </div>
            <div class="data-card">
                <div class="data-label">Acceleration Z</div>
                <div id="accel-z" class="data-value">0.00 g</div>
            </div>
            <div class="data-card">
                <div class="data-label">Total Acceleration</div>
                <div id="total-accel" class="data-value">0.00</div>
            </div>
            <div class="data-card">
                <div class="data-label">Gyroscope X</div>
                <div id="gyro-x" class="data-value">0.00 °/s</div>
            </div>
            <div class="data-card">
                <div class="data-label">Gyroscope Y</div>
                <div id="gyro-y" class="data-value">0.00 °/s</div>
            </div>
            <div class="data-card">
                <div class="data-label">Gyroscope Z</div>
                <div id="gyro-z" class="data-value">0.00 °/s</div>
            </div>
            <div class="data-card">
                <div class="data-label">Gyro Magnitude</div>
                <div id="gyro-mag" class="data-value">0.00</div>
            </div>
        </div>
        
        <div class="chart-container">
            <h3>Acceleration History</h3>
            <canvas id="accel-chart" width="800" height="300"></canvas>
        </div>
        
        <div style="text-align: center;">
            <button class="button" onclick="stopCrashAlert()">Stop Alert</button>
            <button class="button" onclick="clearLog()">Clear Log</button>
        </div>
        
        <div class="chart-container">
            <h3>System Log</h3>
            <div id="log-container" class="log-container"></div>
        </div>
    </div>

    <script>
        let accelData = [];
        let maxDataPoints = 100;
        let canvas = document.getElementById('accel-chart');
        let ctx = canvas.getContext('2d');
        
        function updateData() {
            fetch('/data')
                .then(response => response.json())
                .then(data => {
                    // Update display values
                    document.getElementById('accel-x').textContent = data.ax.toFixed(2) + ' g';
                    document.getElementById('accel-y').textContent = data.ay.toFixed(2) + ' g';
                    document.getElementById('accel-z').textContent = data.az.toFixed(2) + ' g';
                    document.getElementById('total-accel').textContent = data.totalAccel.toFixed(2);
                    document.getElementById('gyro-x').textContent = data.gx.toFixed(2) + ' °/s';
                    document.getElementById('gyro-y').textContent = data.gy.toFixed(2) + ' °/s';
                    document.getElementById('gyro-z').textContent = data.gz.toFixed(2) + ' °/s';
                    document.getElementById('gyro-mag').textContent = data.gyroMag.toFixed(2);
                    
                    // Update status
                    let statusPanel = document.getElementById('status-panel');
                    let statusText = document.getElementById('status-text');
                    let statusDesc = document.getElementById('status-desc');
                    
                    if (data.crashDetected) {
                        statusPanel.className = 'status-panel crash-alert';
                        statusText.textContent = 'CRASH DETECTED!';
                        statusDesc.textContent = 'Emergency alert activated!';
                        addLog('CRASH DETECTED - Alert activated');
                    } else {
                        statusPanel.className = 'status-panel';
                        statusText.textContent = 'System Active';
                        statusDesc.textContent = 'Monitoring for crashes...';
                    }
                    
                    // Update chart data
                    accelData.push({
                        x: data.ax,
                        y: data.ay,
                        z: data.az,
                        total: data.totalAccel
                    });
                    
                    if (accelData.length > maxDataPoints) {
                        accelData.shift();
                    }
                    
                    drawChart();
                })
                .catch(error => {
                    console.error('Error fetching data:', error);
                    addLog('Error: Failed to fetch data');
                });
        }
        
        function drawChart() {
            ctx.clearRect(0, 0, canvas.width, canvas.height);
            
            if (accelData.length < 2) return;
            
            let maxVal = Math.max(...accelData.map(d => Math.abs(d.total))) + 1;
            let minVal = -maxVal;
            
            // Draw grid
            ctx.strokeStyle = '#444';
            ctx.lineWidth = 1;
            for (let i = 0; i <= 10; i++) {
                let y = (canvas.height / 10) * i;
                ctx.beginPath();
                ctx.moveTo(0, y);
                ctx.lineTo(canvas.width, y);
                ctx.stroke();
            }
            
            // Draw acceleration lines
            drawLine(accelData.map(d => d.x), '#ff4444', maxVal, minVal, 'X');
            drawLine(accelData.map(d => d.y), '#44ff44', maxVal, minVal, 'Y');
            drawLine(accelData.map(d => d.z), '#4444ff', maxVal, minVal, 'Z');
            drawLine(accelData.map(d => d.total), '#ffff44', maxVal, minVal, 'Total');
        }
        
        function drawLine(data, color, maxVal, minVal, label) {
            ctx.strokeStyle = color;
            ctx.lineWidth = 2;
            ctx.beginPath();
            
            for (let i = 0; i < data.length; i++) {
                let x = (canvas.width / (maxDataPoints - 1)) * i;
                let y = canvas.height - ((data[i] - minVal) / (maxVal - minVal)) * canvas.height;
                
                if (i === 0) {
                    ctx.moveTo(x, y);
                } else {
                    ctx.lineTo(x, y);
                }
            }
            ctx.stroke();
        }
        
        function stopCrashAlert() {
            fetch('/stop-alert', { method: 'POST' })
                .then(response => response.text())
                .then(result => {
                    addLog('Alert stopped by user');
                });
        }
        
        function clearLog() {
            document.getElementById('log-container').innerHTML = '';
        }
        
        function addLog(message) {
            let logContainer = document.getElementById('log-container');
            let timestamp = new Date().toLocaleTimeString();
            logContainer.innerHTML += `[${timestamp}] ${message}<br>`;
            logContainer.scrollTop = logContainer.scrollHeight;
        }
        
        // Update data every 100ms
        setInterval(updateData, 100);
        
        // Initial load
        updateData();
        addLog('System started - Monitoring active');
    </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  pixels.begin();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(2, OUTPUT);

  pixels.setPixelColor(0, pixels.Color(255, 0, 0));
  pixels.show();

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    pixels.setPixelColor(0, pixels.Color(0, 0, 25));
    pixels.show();
    delay(200);
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    pixels.show();
    delay(200);
  }
  
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  // Setup web server routes
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", html_page);
  });

  server.on("/data", HTTP_GET, []() {
    StaticJsonDocument<300> doc;
    
    doc["ax"] = ax;
    doc["ay"] = ay;
    doc["az"] = az;
    doc["gx"] = gx;
    doc["gy"] = gy;
    doc["gz"] = gz;
    doc["totalAccel"] = Amp / 10.0;
    doc["gyroMag"] = gyroMagnitude;
    doc["crashDetected"] = (millis() < crashDisplayUntil);
    doc["timestamp"] = millis();

    String response;
    serializeJson(doc, response);
    
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", response);
  });

  server.on("/stop-alert", HTTP_POST, []() {
    stopAlert = true;
    crashDisplayUntil = millis();
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "Alert stopped");
  });

  server.begin();
  Serial.println("Web server started");

  Wire.begin();  // I2C initialization
  initMPU();
  
  pixels.setPixelColor(0, pixels.Color(0, 10, 0));
  pixels.show();
}

void loop() {
  server.handleClient();

  if (!mpu_read()) {
    Serial.println("Sensor read error");
    pixels.setPixelColor(0, pixels.Color(15, 15, 15));
    pixels.show();
    delay(1000);
    return;
  }

  processIMUData();
  detectFall();

  if (millis() < crashDisplayUntil) {
    for (int i = 1; i < 10; i++) {
      if (digitalRead(BUTTON_PIN) == LOW) {
        Serial.println("PULL");
        stopAlert = true;
        crashDisplayUntil = millis();
        break;
      }
      Serial.println("CRASH WAS DETECTED");
      pixels.setPixelColor(0, pixels.Color(250, 165, 0));
      pixels.show();
      tone(2, 5000);
      delay(300);
      noTone(2);
      delay(100);
      pixels.setPixelColor(0, pixels.Color(0, 0, 0));
      pixels.show();
      tone(2, 1000);
      delay(300);
      noTone(2);
      delay(100);
    }
    while (!stopAlert) {
      pixels.setPixelColor(0, pixels.Color(150, 0, 0));
      pixels.show();
      tone(2, 5000);
      delay(300);
      noTone(2);
      delay(100);
      pixels.setPixelColor(0, pixels.Color(0, 0, 0));
      pixels.show();
      tone(2, 1000);
      delay(300);
      noTone(2);
      delay(100);
      if (digitalRead(BUTTON_PIN) == LOW) {
        Serial.println("PULL AFTER");
        stopAlert = true;
        crashDisplayUntil = millis();
        break;
      }
    }
  }
  
  pixels.setPixelColor(0, pixels.Color(0, 10, 0));
  pixels.show();
  
  delay(20);
}

void initMPU() {
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
}

void processIMUData() {
  ax = (AcX - 2050) / 16384.0;
  ay = (AcY - 77) / 16384.0;
  az = (AcZ - 1947) / 16384.0;

  gx = (GyX + 270) / 131.07;
  gy = (GyY - 351) / 131.07;
  gz = (GyZ + 136) / 131.07;
}

void detectFall() {
  float Raw_Amp = sqrt(ax * ax + ay * ay + az * az);
  Amp = Raw_Amp * 10;

  gyroMagnitude = sqrt(gx * gx + gy * gy + gz * gz);

  Serial.print("Amp: ");
  Serial.print(Amp);
  Serial.print("\t");
  Serial.print("  Gyro: ");
  Serial.println(gyroMagnitude);

  if (Amp >= 30) {
    crashCount++;
    if (crashCount >= 3) {
      stopAlert = false;
      Serial.println("CRASH WAS DETECTED");
      crashDisplayUntil = millis() + 10000;
      crashCount = 0;
    }
  } else {
    crashCount = 0;
  }
}

bool mpu_read() {
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  Wire.requestFrom(MPU_addr, 14, true);
  if (Wire.available() < 14) {
    return false;
  }
  AcX = Wire.read() << 8 | Wire.read();
  AcY = Wire.read() << 8 | Wire.read();
  AcZ = Wire.read() << 8 | Wire.read();
  Tmp = Wire.read() << 8 | Wire.read();
  GyX = Wire.read() << 8 | Wire.read();
  GyY = Wire.read() << 8 | Wire.read();
  GyZ = Wire.read() << 8 | Wire.read();
  return true;
}