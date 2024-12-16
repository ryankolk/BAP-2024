#include "Proximity.h"
#include <NimBLEDevice.h>
// Configurable I2C Pins
#define I2C_SDA 21 // Default SDA pin
#define I2C_SCL 22 // Default SCL pin

#define SLAVE_ADDR 0x08 // Address of the slave ESP32

// Structure to hold the BLE data

// Array to store the top 10 devices
DeviceRSSI devices[10];

// Function to request data from the slave
void requestDataFromSlave()
{
  uint8_t bytesReceived = Wire.requestFrom(SLAVE_ADDR, 20); // 2 bytes per device (name + RSSI) × 10 devices = 20 bytes

  int index = 0;
  while (Wire.available() && index < 10)
  {
    // Read device name (1 byte for the last three digits)
    // uint8_t  = Wire.read();
    devices[index].deviceName = Wire.read();

    // Read RSSI (1 byte)
    devices[index].rssi = Wire.read();

    // Serial.println("Data requested");
    // Serial.println(devices[index].deviceName);
    // Serial.println(devices[index].rssi);
    index++;
  }
}

void printData()
{
  Serial.println("Top 10 Devices:");
  for (int i = 0; i < 10; i++)
  {
    Serial.print("Device ");
    Serial.print(i + 1);
    Serial.print(": Name: ");
    Serial.print(devices[i].deviceName);
    Serial.print(", RSSI: ");
    Serial.println(devices[i].rssi);
  }
}

void getTopDevices(DeviceRSSI device[10])
{
  for (int i = 0; i < 10; i++)
  {
    device[i] = devices[i];
  }
}

#define BLE_SCAN_INTERVAL 0x80
#define BLE_SCAN_WINDOW 0x80

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice *advertisedDevice)
  {
    // Serial.println(esp_get_free_heap_size());
    // Serial.println(advertisedDevice->getName().c_str());
    // bleStack = uxTaskGetStackHighWaterMark(nullptr);
    // BleFingerprintCollection::Seen(advertisedDevice);
    // ESP_PWR_LVL_P9
  }
};

void scanTask(void *parameter)
{
  NimBLEDevice::init("ESPresense");
  NimBLEDevice::setMTU(23);

  auto pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setInterval(1000);
  pBLEScan->setWindow(500);
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(), true);
  pBLEScan->setActiveScan(false);
  pBLEScan->setDuplicateFilter(false);
  pBLEScan->setMaxResults(0);
  if (!pBLEScan->start(0, nullptr, false))
    Serial.println("Error starting continuous ble scan");

  while (true)
  {
    if (!pBLEScan->isScanning())
    {
      if (!pBLEScan->start(0, nullptr, true))
        Serial.println("Error re-starting continuous ble scan");
      pBLEScan->clearResults();
      Serial.println("re-starting continuous ble scan");
      delay(3000); // If we stopped scanning, don't query for 3 seconds in order for us to catch any missed broadcasts
    }
    else
    {
      delay(100);
    }
  }
}

void ProxmityTask(void *pv)
{
  // pBLEScan = BLEDevice::getScan();
  // pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  // pBLEScan->setActiveScan(false); // Active scan for more detailed results

  while (true)
  {
    // Serial.println("Running proximity task");
    // Create a semaphore lock

    // Take the semaphore before accessing shared resources
    if (xSemaphoreTake(xSemaphore, (TickType_t)10) == pdTRUE)
    {
      // Critical section
      requestDataFromSlave();

      // Give the semaphore back
      xSemaphoreGive(xSemaphore);
      vTaskDelay(500 / portTICK_PERIOD_MS); // Delay between scans
    }
    else
    {
      // Failed to take the semaphore
      // Serial.println("Failed to take semaphore");
      vTaskDelay(10 / portTICK_PERIOD_MS); // Delay between scans
    }
  }
}

// Initialize BLE and start the scanner task
void init_proximity()
{
  // const char *customName = "Dynamic_Node_001"; // Replace with your desired device name
  // BLEDevice::init(customName);                 // Initialize the BLE device with the custom name

  // // Start advertising with the set name
  // BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  // pAdvertising->start();

  xTaskCreatePinnedToCore(
      ProxmityTask,   // Function to run scanTask
      "ProxmityTask", // Name of the task
      3000,           // Stack size
      NULL,           // Task input parameter
      1,              // Priority
      NULL,           // Task handle
      CONFIG_BT_NIMBLE_PINNED_TO_CORE);
}
