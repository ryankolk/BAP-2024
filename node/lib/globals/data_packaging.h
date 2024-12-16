#ifndef DATA_PACKAGING_H
#define DATA_PACKAGING_H

#include "Arduino.h"
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

#endif