#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#include <Wire.h>
#include <SPI.h>

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>  // for BLE Notify

#include <String>

#include "SparkFun_BNO080_Arduino_Library.h"
#include "imubox.h"

#define DEBUG_PRINT
//uncomment to enable IMU_DEBUG from BNO08x library
//#define IMU_DEBUG

// IMU device related variables.
BNO080 IMUDevice;
IMU_Payload_t imuData;

// Declare a variable of type SemaphoreHandle_t.  This is used to reference the
//semaphore that is used to synchronize a task with an interrupt.

SemaphoreHandle_t xIMUDataReadySemaphore;

//IMU interrupt Handler definition
static void IMUInterruptWakeIMUTask( void );

// SPI device
SPIClass spi_port;

//BLE support (for transmission to HUD)
BLEServer* pBLEServer = NULL;
BLECharacteristic* pBLECharacteristic = NULL;
bool deviceConnected = false;

  
//Create Queues for IMU data transfer to BLE
QueueHandle_t IMUqueue;

//IMU queue size (depth)
int IMUqueueSize = 1;

//BLE byte array
char BLEByteArray[100];

//FUNCTIONS
void IMUInit(void);//Sets up IMU
void BLEInit(void);//Sets up BLE
void BoardLEDInit(void);  //Sets up battery voltage measurement
uint8_t ClampU8(uint16_t value);  // Clamp value to max of 255

//RT TASKS
// define tasks for IMU Read, BLE servicing, and HUD transmission
void TaskReadIMU( void *pvParameters );  //Read IMU
void TaskConnectionMonitor( void *pvParameters );  //Monitor BLE
void TaskIMUDataTransmit( void *pvParameters );  //queued output to device

// the setup function runs once when you press reset or power the board
void setup() {

  //Initialize serial communication at 115200 bits per second:
  //Console Serial startup
  Serial.begin(115200);
  
  #ifdef DEBUG_PRINT
  Serial.println("IMUBox Test");
  #endif //DEBUG_PRINT
  
  //Create queue to transfer IMU data to the BLE send task
  IMUqueue = xQueueCreate( IMUqueueSize, sizeof( IMU_Payload_t ) );
  if(IMUqueue == NULL){
  #ifdef DEBUG_PRINT
    Serial.println("Error creating the IMU queue");
  #endif //DEBUG_PRINT  
  }
  IMUInit(); //Initialize IMU system
  BLEInit(); //Initialize BLE system
  BoardLEDInit();  //Board LEDs

  //Before a semaphore is used it must be explicitly created.  
  //This semaphore is used to trigger new data ready from IMU
  //On IMU data interrupt, signal to FreeRTOS that IMU is ready to read
  vSemaphoreCreateBinary( xIMUDataReadySemaphore );  

  /* Check the semaphore was created successfully. */
  if( xIMUDataReadySemaphore != NULL )
  {
    //Initialize Tasks
    //Set up IMU to BLE (Serial initially),100 Hz max rate without custom compile
    //of ESP32 libraries
    xTaskCreatePinnedToCore(
      TaskReadIMU
      ,  "ReadAccelerations"   // Task Name
      ,  1024  // This stack size can be checked & adjusted by reading the Stack Highwater
      ,  NULL
      ,  3  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
      ,  NULL 
      ,  0); //IMU on core 0
  
    //Set up BLE Connection Monitoring Task
     xTaskCreatePinnedToCore(
      TaskConnectionMonitor
      ,  "ConnectionMonitor"
      ,  1024  // Stack size
      ,  NULL
      ,  1  // Priority
      ,  NULL 
      ,  ARDUINO_RUNNING_CORE);
  
     //Set up BLE IMU DataTransmit Task
     xTaskCreatePinnedToCore(
      TaskIMUDataTransmit
      ,  "IMUDataTransmit"
      ,  4096  // Stack size
      ,  NULL
      ,  2  // Priority
      ,  NULL 
      ,  ARDUINO_RUNNING_CORE);//core 1
  
    //connect IMU INT PIN to interrupt falling edge.  On falling edge of IMU data ready
    //pin the interrupt will trigger, which will trigger the semaphore that reads
    //data off the IMU sampling task.
    attachInterrupt(BNOInterruptPin, IMUInterruptWakeIMUTask, FALLING);  


  }
}

void loop()
{
  // Empty. Things are done in Tasks.
}

//Tasks

void TaskReadIMU(void *pvParameters)  // This is a task.
{
  (void) pvParameters;

  volatile byte newLinAcc = 0;
  volatile byte newIMU = 0;
  byte linAccuracy = 0;
  byte imuAccuracy = 0;
  float quatAccuracy=0;

/*
  Reads IMU once notified by semaphore, pipes data into a task using queues
  ~200Hz
*/
  
  Serial.print("IMU Measurement running on core ");
  Serial.println(xPortGetCoreID());

  /* Note that when you create a binary semaphore in FreeRTOS, it is ready
  to be taken, so you may want to take the semaphore after you create it
  so that the task waiting on this semaphore will block until given by
  another task. */
  xSemaphoreTake( xIMUDataReadySemaphore, 0);
  for (;;) // A Task shall never return or exit.
  {
      /* Use the semaphore to wait for the event.  The semaphore was created
      before the scheduler was started so before this task ran for the first
      time.  The task blocks indefinitely meaning this function call will only
      return once the semaphore has been successfully obtained - so there is no
      need to check the returned value. */
     xSemaphoreTake( xIMUDataReadySemaphore, portMAX_DELAY );

    //look for reports from the IMU
     switch (IMUDevice.getReadings())
     {
        case SENSOR_REPORTID_LINEAR_ACCELERATION:
          newLinAcc = 1;
        break;
        case SENSOR_REPORTID_ROTATION_VECTOR:
        break;
        case SENSOR_REPORTID_GAME_ROTATION_VECTOR:
          newIMU=1;
        break;
        case SENSOR_REPORTID_AR_VR_STABILIZED_GAME_ROTATION_VECTOR:
        break;
        default:
           // Unhandled Input Report
           break;
     }
      
      if(newIMU && newLinAcc)  {
        newIMU=0;
        newLinAcc=0;
        IMUDevice.getLinAccel(imuData.Ax, imuData.Ay, imuData.Az, linAccuracy);
        IMUDevice.getQuat(imuData.Qx, imuData.Qy, imuData.Qz, imuData.Real, quatAccuracy, imuAccuracy);
        imuData.SampleCount++;
        xQueueSend(IMUqueue, &imuData, portMAX_DELAY);
      }
  }
}


//Low priority connection monitor task 1Hz
void TaskConnectionMonitor(void *pvParameters)  
{
  (void) pvParameters;
  //setup
  static bool restartAdvertising=false;
  static bool oldDeviceConnected = false;
  #ifdef DEBUG_PRINT
  Serial.print("Connection Monitor running on core ");
  Serial.println(xPortGetCoreID());
  #endif //DEBUG_PRINT

  for (;;)
  {

    //code that helps restart the advertising process for BLE
    //This is only triggered after a disconnect is registered and a delay of
    //one cycle (1 second)
    if(restartAdvertising)
    {
        pBLEServer->startAdvertising(); // restart advertising
 #ifdef DEBUG_PRINT
        Serial.println("restarting advertising");
 #endif //DEBUG_PRINT
        
        restartAdvertising=false;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
  #ifdef DEBUG_PRINT
        Serial.println("BLE reconnected");
  #endif //DEBUG_PRINT
        
    }
    
    //device reconnect
    if (!deviceConnected && oldDeviceConnected) {
      restartAdvertising=true;
#ifdef DEBUG_PRINT
      Serial.println("BLE Disconnected, Waiting to reconnect.");
#endif //DEBUG_PRINT          
      oldDeviceConnected = deviceConnected;
    }

    const TickType_t xDelay = ConnectionSleepTime / portTICK_PERIOD_MS; //# of 10ms ticks
    vTaskDelay(xDelay);
  }
}


void TaskIMUDataTransmit( void *pvParameters )
{
  (void) pvParameters;
#ifdef DEBUG_PRINT
  Serial.print("BLE Write running on core ");
  Serial.println(xPortGetCoreID());
#endif //DEBUG_PRINT  

  for (;;) // A Task shall never return or exit.
  {
    IMU_Payload_t imusample;
  
    for( int i = 0;i < IMUqueueSize;i++ ){
        xQueueReceive(IMUqueue, &imusample, portMAX_DELAY); //from the receive queue, fill the imusample
    }
   
    //Notify BLE (latest queue only)
    // notify changed value
    if (deviceConnected) {
        //Since loop runs at 5msec, send every fourth sample to slow to 20msec for BluetoothLE
        if(0 == (imuData.SampleCount %4))
        {
          pBLECharacteristic->setValue((uint8_t*)&imusample, sizeof(IMU_Payload_t)); //copy from queue to the BLE sending buffer
          pBLECharacteristic->notify(); 
        }
    }
  }
}



void IMUInit(void)
{
  memset(&imuData,0,sizeof(imuData));
  //initialize SPI interface for the BNO085
  spi_port.begin(SPICLK, SPIMISO, SPIMOSI, BNOCSPin);
#ifdef DEBUG_PRINT
  Serial.print("setup() running on core ");
  Serial.println(xPortGetCoreID());
#endif //DEBUG_PRINT  

  // BNO085 Setup
  // enable serial debugging
#ifdef IMU_DEBUG
  IMUDevice.enableDebugging(Serial);
#endif 
  //freeze if IMU is not detected
  if (IMUDevice.beginSPI(BNOCSPin, BNOWakePin, BNOInterruptPin, BNOResetPin, spiDataRate, spi_port) == false)
  {
#ifdef DEBUG_PRINT
    Serial.println("BNO085 over SPI not detected. Halt.");
#endif //DEBUG_PRINT 
    while(1);
  }
  
  // enable accelerometer, readings will be produced every 5 ms
  IMUDevice.enableLinearAccelerometer(BNOAccelUpdateRate);

#ifdef DEBUG_PRINT
  Serial.println("Linear Accelerometer enabled");
  Serial.println("Output in form x, y, z, in m/s^2");
#endif //DEBUG_PRINT 


// enable Game Rotations, readings will be produced every 5 ms
IMUDevice.enableGameRotationVector(BNORotationsUpdateRate);

#ifdef DEBUG_PRINT
  Serial.println("Game Rotations Enabled");
  Serial.println("Output in form Qx,Qy,Qz,Real");
#endif //DEBUG_PRINT 

}

//IMU triggers this event every time a new IMU sample is ready
static void IMUInterruptWakeIMUTask( void )
{
  static signed portBASE_TYPE xHigherPriorityTaskWoken;
  xHigherPriorityTaskWoken = pdFALSE;
  /* 'Give' the semaphore to unblock the task. */
  //This signals IMU task to read data
  xSemaphoreGiveFromISR( xIMUDataReadySemaphore, (signed portBASE_TYPE*)&xHigherPriorityTaskWoken );
}

// BLE SETUP ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

//BLE Server callbacks:
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pBLEServer) {
      deviceConnected = true;
      //set green if device connected
      digitalWrite(UserLED1, 1);

    };

    void onDisconnect(BLEServer* pBLEServer) {
      deviceConnected = false;
      //turn LED off on disconnect
      digitalWrite(UserLED1, 0);
    }
};

//BLE Characteristic received callback
//Triggers on message receipt over BLE
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pBLECharacteristic) {
      std::string value = pBLECharacteristic->getValue();
      size_t msg_length = value.length();
      strcpy(BLEByteArray,value.c_str());
      switch(BLEByteArray[0])
      {
        //set up for command processing later
        default:;
      }
    }
};


//BLE system initialization
void BLEInit(void) {
#ifdef DEBUG_PRINT
  Serial.println("Setting up BLE device: IMUBox");
#endif //DEBUG_PRINT 
  
  BLEDevice::init(BLE_DEV_NAME); 
  //Server setup and callbacks
  pBLEServer = BLEDevice::createServer();
  //Server callbacks, different than characteristic callback below
  pBLEServer->setCallbacks(new MyServerCallbacks());
  //service setup
  BLEService *pBLEService = pBLEServer->createService(BLE_SERV_UUID);

  // Create a BLE Characteristic
  pBLECharacteristic = pBLEService->createCharacteristic(
                      BLE_CHAR_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  pBLECharacteristic->addDescriptor(new BLE2902());
  //Characteristic callbacks
  pBLECharacteristic->setCallbacks(new MyCallbacks());
  //Start the service
  pBLEService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(BLE_SERV_UUID);
  pAdvertising->setScanResponse(true);
  //pAdvertising->setMinInterval(0x10);  // set value to 0x00 to not advertise this parameter
  //pAdvertising->setMaxInterval(0x10);  // set value to 0x00 to not advertise this parameter
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMaxPreferred(0x10);
  BLEDevice::startAdvertising();
#ifdef DEBUG_PRINT
  Serial.println("Waiting for BLE connection");
#endif //DEBUG_PRINT 
}

//Sets up Board LEDs for use
void BoardLEDInit(void)
{

  pinMode(UserLED1, OUTPUT);
  pinMode(UserLED2, OUTPUT);
  pinMode(UserLED3, OUTPUT);
  pinMode(UserLED4, OUTPUT);
  digitalWrite(UserLED1, 0);
  digitalWrite(UserLED2, 0);
  digitalWrite(UserLED3, 0);
  digitalWrite(UserLED4, 0);
}



//Sets max value at 255 for the LED combined functions
uint8_t ClampU8(uint16_t u16_value)
{
  if(u16_value > 255)
  {
    return 255;
  }
  else
  {
    return (uint8_t)(u16_value);
  }
}
