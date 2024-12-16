
#include "sensors/Proximity.h"
#include "sensors/Microphone.h"
#include "sensors/Accelerometer.h"
#include "data_packaging.h"
#include "ESP32Time.h"
#include <globals.h>
#include "zh_network.h"

SemaphoreHandle_t xSemaphore = xSemaphoreCreateMutex();

void sensorSetup()
{
  // Initialize accelerometer and microphone (and start microphone)
  setupAccelerometer();
  // Calibrate accelerometer
  calibrateAccelerometer();

  setupMicrophone();

  // Start the accelerometer task
  startAccelerometerTask(); // Start the accelerometer task from accelerometer.ino

  // Initialize BLE functionality (and start proximity sensing)
  init_proximity();
}

void sensorLoopTask()
{
  static unsigned long lastRunTime = 0; // Store the last time the loop ran

  // Check if 1000ms (1 second) has passed since the last execution
  if (millis() - lastRunTime >= 1000)
  {
    lastRunTime = millis(); // Update the last run time

    // Fetch microphone data (including zero-crossings)
    MicrophoneData microphonedata = {
        .avgDb = 0,
        .peakFrequency = 0,
        .zeroCrossingsCount = 0,
    };

    double avgDb;
    double peakestFrequency;
    int zeroCrossingsCount;
    if (getMicrophoneData(avgDb, peakestFrequency, zeroCrossingsCount))
    {

      // put microphone data in microphone struct
      microphonedata.avgDb = static_cast<uint16_t>(avgDb);
      microphonedata.peakFrequency = static_cast<uint16_t>(peakestFrequency);
      microphonedata.zeroCrossingsCount = static_cast<uint16_t>(zeroCrossingsCount);
    }
    else
    {
      Serial.println("Microphone data not ready.");
    }

    // Fetch accelerometer data
    float roll, pitch, yaw;
    getAccelerometerData(roll, pitch, yaw);

    // put accelerometer data in acceleerometer struct
    AccelerometerData accelerometerdata = {0};
    accelerometerdata.roll = roll;
    accelerometerdata.pitch = pitch;
    accelerometerdata.yaw = yaw;

    Serial.println(esp_get_free_heap_size());
    // Print timestamp
    Serial.println("-----------------------");
    Serial.printf("Timestamp: %lu ms\n", millis());
    // Serial.println("-----------------------");

    OutputData data = {0};
    // Create arrays to store device names and RSSI values
    // Get the top 10 devices and their RSSI values
    getTopDevices(data.bleData);

    // for (int c = 0; j < 10; j++)
    // {
    //   bledata.deviceNames[j] = 0;
    //   bledata.rssiValues[j] = 0;
    // }

    // make final output data struct
    data.microphoneData = microphonedata;
    data.accelerometerData = accelerometerdata;

    // Print MicrophoneData
    Serial.print(data.microphoneData.avgDb);
    Serial.print(", ");
    Serial.print(data.microphoneData.peakFrequency);
    Serial.print(", ");
    Serial.println(data.microphoneData.zeroCrossingsCount);

    // Print AccelerometerData
    Serial.print(data.accelerometerData.roll, 2);
    Serial.print(", ");
    Serial.print(data.accelerometerData.pitch, 2);
    Serial.print(", ");
    Serial.println(data.accelerometerData.yaw, 2);

    // Print BLEData
    Serial.print("[");
    for (int i = 0; i < 10; ++i)
    {
      Serial.print(data.bleData[i].deviceName);
      if (i < 9)
        Serial.print(", ");
    }
    Serial.print("], [");
    for (int i = 0; i < 10; ++i)
    {
      Serial.print(data.bleData[i].rssi);
      if (i < 9)
        Serial.print(", ");
    }
    Serial.println("]");
    Serial.println("-----------------------");

    message_t send_message;
    send_message.message_header = {
        .type = DATA,
        .id = 0,
        .timestamp = rtc.getEpoch(),
        .timestamp_us = rtc.getMicros()};
    send_message.data = data;

    zh_network_send(target, (uint8_t *)&send_message, sizeof(send_message));
  }
}
