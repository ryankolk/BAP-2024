#include <Arduino.h>

#define MESSAGE_LENGTH sizeof(message_t)
#define QUEUE_SIZE 150 // Increased queue size to handle more messages
#define BATCH_SIZE 50  // Further reduced batch size for reliability
struct MicrophoneData
{
  uint16_t avgDb;
  uint16_t peakFrequency;
  uint16_t zeroCrossingsCount;
};
// Struct to hold accelerometer data
struct AccelerometerData
{
  float roll;
  float pitch;
  float yaw;
};
struct DeviceRSSI
{
  uint8_t deviceName; // Last three digits of the device name
  uint8_t rssi;       // RSSI value
};

unsigned long missed = 0;
unsigned long total = 0;

// Struct to hold BLE data (top 10 devices and their RSSI)
// struct BLEData
// {
//   DeviceRSSI device[10];
// };

// Struct to hold all relevant output data
struct OutputData
{
  MicrophoneData microphoneData;
  AccelerometerData accelerometerData;
  DeviceRSSI bleData[10];
};
typedef enum // Enumeration of possible status of sent ESP-NOW message.
{
  SYNC_REQUEST,
  SYNC_RESPONSE,
  HEARTBEAT,
  MESSAGE,
  RESET_TIME,
  DATA
} message_type_t;
typedef struct
{
  message_type_t type;
  uint8_t id;
  unsigned long timestamp;
  unsigned long timestamp_us;
} message_header_t;

// Defining message structure
typedef struct
{
  message_header_t message_header;
  union
  {
    OutputData data;
    // message_heartbeat heartbeat;
    // uint8_t data[200 - sizeof(message_header)];
  };
} message_t;

message_t message;

message_t messageQueue[QUEUE_SIZE];
uint8_t queueHead = 0;
uint8_t queueTail = 0;

bool isQueueFull()
{
  return ((queueHead + 1) % QUEUE_SIZE) == queueTail;
}

bool isQueueEmpty()
{
  return queueHead == queueTail;
}

bool enqueueMessage(const message_t &message)
{
  if (isQueueFull())
  {
    return false;
  }
  messageQueue[queueHead] = message;
  queueHead = (queueHead + 1) % QUEUE_SIZE;
  return true;
}

bool dequeueMessage(message_t &message)
{
  if (isQueueEmpty())
  {
    return false;
  }
  message = messageQueue[queueTail];
  queueTail = (queueTail + 1) % QUEUE_SIZE;
  return true;
}

uint16_t checksumCalculator(uint8_t *data, uint16_t length)
{
  uint16_t crc = 0xFFFF;        // Initial value
  uint16_t polynomial = 0x1021; // Polynomial used in CRC-16-CCITT

  for (uint16_t i = 0; i < length; i++)
  {
    crc ^= (data[i] << 8); // XOR the highest byte with the data byte

    for (uint8_t bit = 0; bit < 8; bit++)
    {
      if (crc & 0x8000)
      {
        crc = (crc << 1) ^ polynomial;
      }
      else
      {
        crc <<= 1;
      }
    }
  }

  return crc;
}

void createDummyData(void *pv)
{
  message_t message;
  message.message_header.type = DATA;

  while (true)
  {
    message.message_header.timestamp = missed;
    message.message_header.timestamp_us = total;
    total++;
    if (!enqueueMessage(message))
    {
      missed++;
      delay(2);
    }
    else
    {
      delay(2);
    }
  }
}

void setup()
{
  Serial.begin(115200);

  // Flush any unwanted data in the serial buffer
  while (Serial.available() > 0)
  {
    Serial.read();
  }

  // Send ready byte to indicate ESP32 is ready for communication
  Serial.write(0x01); // Ready byte

  xTaskCreatePinnedToCore(
      createDummyData,   /* Task */
      "createDummyData", /* Name of the task */
      10000,             /* Stack size */
      NULL,              /* Parameter of the task */
      1,                 /* Priority of the task */
      NULL,              /* Task handle. */
      1);                /* Core where the task should run */
}

void loop()
{
  static uint8_t resendCount = 0;
  uint8_t messages[MESSAGE_LENGTH * BATCH_SIZE];
  bool send = false;
  unsigned long commandStartTime = millis();

  // Retry mechanism for receiving the start byte
  int retryAttempts = 3;
  while (retryAttempts > 0)
  {
    // Wait for the start byte
    while (millis() - commandStartTime < 1000)
    {
      if (Serial.available() > 0)
      {
        uint8_t command = Serial.read();
        if (command == 0x25)
        {
          send = true;
          // delay(10);
          break;
        }
        else if (command == 0x11)
        {
          uint64_t currentTime = micros();
          // read rest of the data from serial port
          //  Serial.write((uint8_t *)&currentTime, 8);
          Serial.write(0x07);
          delay(10);
        }
      }
    }

    if (send)
    {
      break;
    }
    else
    {
      retryAttempts--;
      delay(200);
    }
  }

  if (!send)
  {
    return;
  }

  // Prepare batch of messages to send
  uint16_t length = 0;
  uint8_t batchCount = 0;
  while (batchCount < BATCH_SIZE && dequeueMessage(*reinterpret_cast<message_t *>(&messages[length])))
  {
    length += MESSAGE_LENGTH;
    batchCount++;
  }

  if (batchCount > 0)
  {
    uint16_t crc = checksumCalculator(messages, length);
    // Serial.write((uint8_t)length);
    Serial.write((uint8_t *)&length, 2);
    Serial.write((uint8_t *)&crc, 2);
    Serial.write(messages, length);

    // Wait for acknowledgment with a longer timeout and non-blocking delay
    unsigned long startTime = millis();
    bool ackReceived = false;
    while (millis() - startTime < 1000)
    {
      if (Serial.available() > 0)
      {
        uint8_t response = Serial.read();
        if (response == 0x07)
        {
          ackReceived = true;
          resendCount = 0;
          // Serial.write(0x06);
          // delay(1);
          break;
        }
        // delay(1);
      }
      // delay(1);
    }
    if (!ackReceived)
    {
      resendCount++;
      if (resendCount >= 3)
      {
        resendCount = 0;
      }
    }
  }
  else
  {
    resendCount = 0;
  }
}
