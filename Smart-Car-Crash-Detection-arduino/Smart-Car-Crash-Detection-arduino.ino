#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <math.h>
#include <Adafruit_NeoPixel.h>


BLEServer* pServer = NULL;
BLECharacteristic* alertCharacteristic  = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

#define BUTTON_PIN 4
bool stopAlert = false;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define PIN 48
#define NUMPIXELS 1

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
#define DELAYVAL 500


#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define ALERT_CHAR_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"


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


class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    Serial.println("connected");
    deviceConnected = true;
  };

  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
  }
};



void setup() {
  Serial.begin(115200);
  pixels.begin();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(2, OUTPUT);

  pixels.setPixelColor(0, pixels.Color(255, 0, 0));
  pixels.show();
  // Create the BLE Device
  BLEDevice::init("Crash Detection");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService* pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic for temperature
  alertCharacteristic  = pService->createCharacteristic(
    ALERT_CHAR_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_INDICATE);

  // Create a BLE Descriptor
  alertCharacteristic ->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a connection...");


  while (!deviceConnected) {
    pixels.setPixelColor(0, pixels.Color(0, 0, 25));
    pixels.show();
    delay(200);
    pixels.setPixelColor(0, pixels.Color(0, 0, 0));
    pixels.show();
    delay(200);
  }



  Wire.begin();  // I2C initialization (default: SDA=8, SCL=9 on ESP32-S3)
  initMPU();
}

void loop() {
  // notify changed value
  if (deviceConnected) {
    pixels.setPixelColor(0, pixels.Color(0, 10, 0));
    pixels.show();

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
        alertCharacteristic ->setValue("CRASH WAS DETECTED");
        alertCharacteristic ->notify();
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
    crashDisplayUntil = millis();
    Serial.println(stopAlert);
    delay(20);
  }
  // disconnecting
  if (!deviceConnected && oldDeviceConnected) {
    pServer->startAdvertising();  // restart advertising
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
    while (!deviceConnected) {
      pixels.setPixelColor(0, pixels.Color(0, 0, 25));
      pixels.show();
      delay(200);
      pixels.setPixelColor(0, pixels.Color(0, 0, 0));
      pixels.show();
      delay(200);
    }
  }

  // connecting
  if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }

  delay(100);
}

void initMPU() {
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
}

// ========== Process Sensor Data ========== //
void processIMUData() {
  ax = (AcX - 2050) / 16384.0;
  ay = (AcY - 77) / 16384.0;
  az = (AcZ - 1947) / 16384.0;

  gx = (GyX + 270) / 131.07;
  gy = (GyY - 351) / 131.07;
  gz = (GyZ + 136) / 131.07;
}

// ========== Updated Crash Detection Algorithm ========== //
void detectFall() {
  float Raw_Amp = sqrt(ax * ax + ay * ay + az * az);
  Amp = Raw_Amp * 10;
  char buffer[6];

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

  sprintf(buffer, "%02d", 0);
  alertCharacteristic ->setValue(buffer);
  alertCharacteristic ->notify();
}

// ========== Read Sensor Data from MPU6050 ========== //
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
