#include "serial.h"
#include "zh_network.h"
#include "ESP32Time.h"
#define MESSAGE_LENGTH sizeof(message_t)
#define QUEUE_SIZE 20
#define BATCH_SIZE 2 // Further reduced batch size for reliability

// message_t message = {0};

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
  // int test = sizeof(message_header_t)
  // messageQueue[queueHead] = message;
  memcpy(&messageQueue[queueHead], &message, sizeof(message_t));
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

void processSerial()
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
          // uint64_t currentTime = micros();
          uint8_t buffer[8];
          Serial.readBytes(buffer, 8);
          uint32_t firstValue = *reinterpret_cast<uint32_t *>(buffer);
          uint32_t secondValue = *reinterpret_cast<uint32_t *>(buffer + 4);
          rtc.setTime(firstValue, secondValue);
          timeIsSynced = true;
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
    Serial.write((uint8_t)length);
    Serial.write((uint8_t)length >> 8);
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
          delay(10);
          break;
        }
      }
      delay(10);
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