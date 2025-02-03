#include <Wire.h>
#include <NimBLEDevice.h>
#include <math.h>

// BLE UUID definitions (you can change these if desired)
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcdefab-1234-1234-1234-abcdefabcdef"

// MPU6050 I2C address
const int MPU_addr = 0x68;

// Raw sensor data variables
int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;

// Processed sensor data (accelerometer in g's and gyroscope in °/s)
float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;

// Fall detection state flags and counters
boolean fall = false;
boolean trigger1 = false;
boolean trigger2 = false;
boolean trigger3 = false;
byte trigger1count = 0;
byte trigger2count = 0;
byte trigger3count = 0;
int angleChange = 0;

// BLE characteristic pointer (for sending notifications)
NimBLECharacteristic* pCharacteristic;

//
// BLE Setup: Initializes the BLE server, service, and characteristic.
//
void setupBLE() {
  NimBLEDevice::init("ESP32_Fall_Detect_BLE");  // Set device name
  NimBLEServer* pServer = NimBLEDevice::createServer();
  NimBLEService* pService = pServer->createService(SERVICE_UUID);
  
  pCharacteristic = pService->createCharacteristic(
                       CHARACTERISTIC_UUID,
                       NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
                     );
  pCharacteristic->setValue("No Alert");
  pService->start();

  // Create advertisement data for the scan response
  NimBLEAdvertisementData scanResponseData;
  scanResponseData.setName("ESP32_Fall_Detect_BLE");

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponseData(scanResponseData);  // Use setScanResponseData()
  pAdvertising->start();
  Serial.println("BLE advertising started");
}

//
// Setup: Initializes Serial, I2C (for MPU6050), BLE, and the MPU6050 sensor.
//
void setup() {
  Serial.begin(115200);
  Wire.begin();
  
  setupBLE();
  initMPU();
}

//
// Main loop: Reads sensor data, processes it, and checks for a fall.
// If a fall is detected, a BLE notification is sent.
//
void loop() {
  if (!mpu_read()) {
    Serial.println("Sensor read error");
    delay(1000);
    return;
  }
  
  processIMUData();
  detectFall();
  delay(100);
}

//
// Initializes the MPU6050 sensor
//
void initMPU() {
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);  // PWR_MGMT_1 register
  Wire.write(0);     // Wake up the MPU6050
  if (Wire.endTransmission(true) != 0) {
    Serial.println("MPU6050 init error");
  } else {
    Serial.println("MPU6050 initialized");
  }
}

//
// Converts raw sensor readings into units (g and °/s)
//
void processIMUData() {
  ax = (AcX - 2050) / 16384.0;
  ay = (AcY - 77) / 16384.0;
  az = (AcZ - 1947) / 16384.0;
  gx = (GyX + 270) / 131.07;
  gy = (GyY - 351) / 131.07;
  gz = (GyZ + 136) / 131.07;
}

//
// Fall detection algorithm: Checks acceleration and orientation changes.
// When a crash is detected, it sends a BLE notification.
//
void detectFall() {
  float Raw_Amp = sqrt(ax * ax + ay * ay + az * az);
  int Amp = Raw_Amp * 10;
  Serial.println(Amp);

  if (Amp <= 2 && !trigger2) {
    trigger1 = true;
    Serial.println("Trigger 1 activated");
  }
  
  if (trigger1) {
    trigger1count++;
    if (Amp >= 12) {
      trigger2 = true;
      Serial.println("Trigger 2 activated");
      trigger1 = false;
      trigger1count = 0;
    }
  }
  
  if (trigger2) {
    trigger2count++;
    angleChange = sqrt(gx * gx + gy * gy + gz * gz);
    Serial.println(angleChange);
    if (angleChange >= 30 && angleChange <= 400) {
      trigger3 = true;
      trigger2 = false;
      trigger2count = 0;
      Serial.println("Trigger 3 activated");
    }
  }
  
  if (trigger3) {
    trigger3count++;
    if (trigger3count >= 10) {
      angleChange = sqrt(gx * gx + gy * gy + gz * gz);
      Serial.println(angleChange);
      if (angleChange >= 0 && angleChange <= 10) {
        fall = true;
        trigger3 = false;
        trigger3count = 0;
        Serial.println("CRASH WAS DETECTED");
        // Send BLE notification with the alert message
        pCharacteristic->setValue("CRASH WAS DETECTED");
        pCharacteristic->notify();
        fall = false;
      } else {
        trigger3 = false;
        trigger3count = 0;
        Serial.println("Trigger 3 deactivated");
      }
    }
  }
  
  if (trigger2count >= 6) {
    trigger2 = false;
    trigger2count = 0;
    Serial.println("Trigger 2 deactivated");
  }
  
  if (trigger1count >= 6) {
    trigger1 = false;
    trigger1count = 0;
    Serial.println("Trigger 1 deactivated");
  }
}

//
// Reads sensor data from the MPU6050. Returns true if successful.
//
bool mpu_read() {
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B);  // Starting register for accelerometer data
  if (Wire.endTransmission(false) != 0) {
    return false; // Sensor not detected
  }
  Wire.requestFrom(MPU_addr, 14, true);
  if (Wire.available() < 14) {
    return false; // Data not received properly
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
