#ifndef IMUBOX_H
#define IMUBOX_H

// BNO085 control
// BNO085 pins
const byte BNOCSPin = 5;
const byte BNOWakePin = 27;
const byte BNOInterruptPin = 34;
const byte BNOResetPin = 25;

// SPI port pins
const uint32_t spiDataRate = 3000000;
const byte SPICLK = 18;
const byte SPIMISO = 19;
const byte SPIMOSI = 23;

// Accelerometer read/notify rate
const uint16_t BNOAccelUpdateRate =5;
// IMU rotations update rate
const uint16_t BNORotationsUpdateRate =5;

//BLE Read Constants
const byte I2CSDA=21;
const byte I2CSCL=22;

// Setup BLE specific IDs

#define BLE_SERV_UUID    "D587BB16-6DAC-4BC3-B3EE-601B2E72C880"
#define BLE_CHAR_UUID    "1DD852AD-7CBD-4A54-875A-CE3D10F9340A"
#define BLE_DEV_NAME     "IMUBox"

//LEDs
const byte NeoPixelPin = 21;

const byte UserLED1 = 12;
const byte UserLED2 = 14;
const byte UserLED3 = 13;
const byte UserLED4 = 15;

// BLE loop Sleep time
const unsigned long ConnectionSleepTime = 1000; //msec

typedef struct {
  //consider adding orientations and rates
  float Qx;
  float Qy;
  float Qz;
  float Real;
	float Ax;
	float Ay;
	float Az;
  uint32_t SampleCount;
}IMU_Payload_t;


#endif //IMUBOX_H
