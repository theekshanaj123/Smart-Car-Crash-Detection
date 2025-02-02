// IoT-based Fall Detection using ESP32 and MPU6050 Sensor
#include <Wire.h>
#include <WiFi.h>

const int MPU_addr = 0x68;  // I2C address of the MPU-6050
int16_t AcX, AcY, AcZ, Tmp, GyX, GyY, GyZ;
float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
boolean fall = false;
boolean trigger1 = false;
boolean trigger2 = false;
boolean trigger3 = false;
byte trigger1count = 0;
byte trigger2count = 0;
byte trigger3count = 0;
int angleChange = 0;

void setup() {
    Serial.begin(115200);
    Wire.begin();
    Wire.beginTransmission(MPU_addr);
    Wire.write(0x6B);
    Wire.write(0);
    if (Wire.endTransmission(true) != 0) {
        Serial.println("SENSOR ERROR: CRASH DETECTED");
    } else {
        Serial.println("Wrote to IMU");
    }
}

void loop() {
    if (!mpu_read()) {
        Serial.println("SENSOR ERROR: CRASH DETECTED");
        delay(1000);
        return;
    }

    ax = (AcX - 2050) / 16384.00;
    ay = (AcY - 77) / 16384.00;
    az = (AcZ - 1947) / 16384.00;
    gx = (GyX + 270) / 131.07;
    gy = (GyY - 351) / 131.07;
    gz = (GyZ + 136) / 131.07;

    float Raw_Amp = sqrt(ax * ax + ay * ay + az * az);
    int Amp = Raw_Amp * 10;
    Serial.println(Amp);

    if (Amp <= 2 && !trigger2) {
        trigger1 = true;
        Serial.println("TRIGGER 1 ACTIVATED");
    }

    if (trigger1) {
        trigger1count++;
        if (Amp >= 12) {
            trigger2 = true;
            Serial.println("TRIGGER 2 ACTIVATED");
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
            Serial.println("TRIGGER 3 ACTIVATED");
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
                Serial.println("CRASH DETECTED");
                fall = false;
            } else {
                trigger3 = false;
                trigger3count = 0;
                Serial.println("TRIGGER 3 DEACTIVATED");
            }
        }
    }

    if (trigger2count >= 6) {
        trigger2 = false;
        trigger2count = 0;
        Serial.println("TRIGGER 2 DEACTIVATED");
    }

    if (trigger1count >= 6) {
        trigger1 = false;
        trigger1count = 0;
        Serial.println("TRIGGER 1 DEACTIVATED");
    }

    delay(100);
}

bool mpu_read() {
    Wire.beginTransmission(MPU_addr);
    Wire.write(0x3B);
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
