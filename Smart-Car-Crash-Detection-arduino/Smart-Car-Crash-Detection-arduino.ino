#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>
#include <WiFi.h>

// ========== OLED Display Configuration ========== //
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1    // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ========== MPU6050 Configuration ========== //
const int MPU_addr = 0x68; // I2C address for MPU6050

// Raw sensor data variables
int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ, Amp, gyroMagnitude;

// Processed sensor data (accelerometer in g's and gyroscope in °/s)
float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;

// Global variable for crash detection count (to filter out noise)
int crashCount = 0;

// Global variable to hold the time until which "CRASH DETECTED" is shown
unsigned long crashDisplayUntil = 0;

// ========== OLED Display Setup ==========
void setupOLED()
{
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println("OLED allocation failed");
    while (true)
      ;
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("OLED Initialized");
  display.display();
  delay(2000);
}

// ============ Connect to WIFI=========== //
void setupWifi()
{
}

// ========== Setup ========== //
void setup()
{
  Serial.begin(115200);
  Wire.begin(); // I2C initialization (default: SDA=8, SCL=9 on ESP32-S3)

  setupOLED();
  initMPU();
}

// ========== Main Loop ========== //
void loop()
{

  if (!mpu_read())
  {
    Serial.println("Sensor read error");
    // showError(0);
    delay(1000);
    return;
  }

  processIMUData();
  detectFall();
  updateOLED();
  delay(100);
}

// ========== MPU6050 Initialization ========== //
void initMPU()
{
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
}

// ========== Process Sensor Data ========== //
void processIMUData()
{
  ax = (AcX - 2050) / 16384.0;
  ay = (AcY - 77) / 16384.0;
  az = (AcZ - 1947) / 16384.0;

  gx = (GyX + 270) / 131.07;
  gy = (GyY - 351) / 131.07;
  gz = (GyZ + 136) / 131.07;
}

// ========== Updated Crash Detection Algorithm ========== //
void detectFall()
{
  float Raw_Amp = sqrt(ax * ax + ay * ay + az * az);
  Amp = Raw_Amp * 10;

  gyroMagnitude = sqrt(gx * gx + gy * gy + gz * gz);

  Serial.print("Amp: ");
  Serial.print(Amp);
  Serial.print("\t");
  Serial.print("  Gyro: ");
  Serial.println(gyroMagnitude);

  if (Amp >= 60)
  {
    crashCount++;
    if (crashCount >= 3)
    {
      Serial.println("CRASH WAS DETECTED");
      crashDisplayUntil = millis() + 10000;
      crashCount = 0;
    }
  }
  else
  {
    crashCount = 0;
  }
}

// ========== Read Sensor Data from MPU6050 ========== //
bool mpu_read()
{
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0)
  {
    return false;
  }
  Wire.requestFrom(MPU_addr, 14, true);
  if (Wire.available() < 14)
  {
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

// ========== Update OLED Display ========== //
void updateOLED()
{
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);

  if (millis() < crashDisplayUntil)
  {
    display.setTextSize(2);
    display.println("CRASH");
    display.println("DETECTED!");
  }
  else
  {
    float Raw_Amp = sqrt(ax * ax + ay * ay + az * az);
    int Amp = Raw_Amp * 10;

    display.print("Amp: ");
    display.println(Amp);
    display.print("Gyro: ");
    display.println((int)sqrt(gx * gx + gy * gy + gz * gz));
    display.println("Status: Normal");

    display.print("ax: ");
    display.println(ax, 2);
    display.print("ay: ");
    display.println(ay, 2);
    display.print("az: ");
    display.println(az, 2);
  }

  display.display();
}

// ======== Show Error ========= //
void showError(int e)
{
  switch (e)
  {
  case 0:
    Serial.println("sensor error");
    break;

  case 1:
    break;

  default:
    break;
  }
}
