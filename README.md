# 🚗 ESP32 BLE Crash Detection System

This project is a **crash detection system** using an **ESP32**, **MPU6050**, **NeoPixel LED**, **buzzer**, and **Bluetooth Low Energy (BLE)** communication. When a crash is detected via accelerometer and gyroscope data, the system notifies a connected mobile device and triggers alerts through a buzzer and NeoPixel LED.

## 🧠 Features

- BLE connection to mobile devices
- Real-time crash detection using MPU6050 sensor data
- Alerts via LED flashing and buzzer sounds
- Crash notification sent to BLE client
- Emergency stop using a push button

## 🛠️ Hardware Used

- ESP32-S3
- MPU6050 (Accelerometer + Gyroscope)
- Adafruit NeoPixel
- Piezo buzzer
- Push button
- USB cable and breadboard

## 📷 Circuit Connections

| Component        | ESP32 Pin   |
|------------------|-------------|
| MPU6050 SDA      | GPIO 8      |
| MPU6050 SCL      | GPIO 9      |
| NeoPixel DIN     | GPIO 48     |
| Buzzer           | GPIO 2      |
| Push Button      | GPIO 4      |

> **Note**: Make sure you connect appropriate pull-up resistors if needed for I2C lines.

## 📦 Libraries Required

Install these libraries via Arduino Library Manager:

- `Adafruit GFX`
- `Adafruit NeoPixel`
- `BLEDevice` (ESP32 BLE support)
- `Wire` (for I2C)

## 🔧 Setup & Upload

1. Install [Arduino IDE](https://www.arduino.cc/en/software)
2. Add ESP32 Board via **Boards Manager**
3. Open this code and select your board (e.g., `ESP32-S3 Dev Module`)
4. Connect your ESP32 and upload the sketch

## 📡 BLE Characteristics

- **Service UUID:** `4fafc201-1fb5-459e-8fcc-c5c9c331914b`
- **Characteristic UUID:** `beb5483e-36e1-4688-b7f5-ea07361b26a8`
- BLE device name: **Crash Detection**

## 🧪 Crash Detection Logic

- Acceleration magnitude `Amp` and gyroscope magnitude are calculated.
- If `Amp ≥ 30` for 3 consecutive checks, a crash is detected.
- An alert is shown for 10 seconds, with LED blinking and buzzer sounds.
- A BLE notification message `"CRASH WAS DETECTED"` is sent.
- The button can be pressed to stop the alert.

## 📲 Mobile App Integration

- Any BLE scanner app can be used to test (e.g., **nRF Connect**, **LightBlue**).
- Connect to the device and monitor the characteristics for crash messages.

## 👨‍👩‍👧‍👦 Team Members

- **W.A.D.T.J Wijethunga**
- **D.M.M.M Dasanayake**
- **N.P.T.H Nishshanka**

### 👨‍🏫 Supervisor

- **Mr. Priyashantha Tennakoon**, Lecturer

