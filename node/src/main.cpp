#include "nvs_flash.h"
#include "esp_netif.h"
#include "zh_network.h"
#include <ESP32Time.h>
#include "Arduino.h"
#ifdef PERF
#include "perf.h"
#endif
#include "globals.h"
#ifdef STATIC
#include <NimBLEDevice.h>
#else
#include "sensors/sensorTask.h"
#endif
#ifdef ROOT_NODE
#include "serial.h"
#endif
#include <WiFi.h>
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
bool bleIsActive = false;
bool timeIsSynced = false;
unsigned long timeTimer = 0;

void zh_network_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);

// uint8_t target[6] = {0xD8, 0xBC, 0x38, 0xE3, 0xFF, 0xF4};
// uint8_t target[6] = {0x24, 0x6F, 0x28, 0x0B, 0x94, 0xF8};
uint8_t target[6] = {0x88, 0x13, 0xBF, 0x09, 0x90, 0x94};

#ifdef DEBUG
String printEpochAsDateTime(unsigned long epoch, unsigned long offset)
{
  struct tm timeinfo;
  time_t now = epoch;
  localtime_r(&now, &timeinfo);
  time_t tt = mktime(&timeinfo);

  char s[51];
  strftime(s, 50, "%a, %b %d %Y %H:%M:%S", &timeinfo);
  printf("%s:%d\n", s, offset);
  return String(s);
}
#endif

void requestTimeSync(void *pv)
{
  while (true)
  {
    if ((!timeIsSynced && (millis() - timeTimer) > 5000) || (timeIsSynced && (millis() - timeTimer) > 3600000))
    {
      timeIsSynced = false;
      printf("Requesting time sync\n");
      timeTimer = millis();

      message_t send_message;
      send_message.message_header = {
          .type = SYNC_REQUEST,
          .timestamp = rtc.getEpoch(),
          .timestamp_us = rtc.getMicros()};
      send_message.sync_request = {};

      zh_network_send(target, (uint8_t *)&send_message, sizeof(send_message));
    }
    delay(1000);
  }
}

uint32_t i = 0;
uint32_t last_i = 0;

void sendIteration(void *pv)
{
  while (true)
  {
    message_t send_message;
    send_message.message_header = {
        .type = MESSAGE,
        .timestamp = rtc.getEpoch(),
        .timestamp_us = rtc.getMicros()};
    send_message.message = {
        .value = i};
    i++;
    printf("Sending message %d\n", i);

    zh_network_send(target, (uint8_t *)&send_message, sizeof(send_message));
    // print heap of esp
    // printf("heap: %d\n", esp_get_free_heap_size());
    delay(1000);
  }
}

void setup()
{
  WiFi.mode(WIFI_STA);
#if defined(DEBUG) || defined(ROOT_NODE)
  Serial.begin(115200);
  while (Serial.available() > 0)
  {
    Serial.read();
  }
  // Send ready byte to indicate ESP32 is ready for communication
  Serial.write(0x01); // Ready byte
#endif
  // setCpuFrequencyMhz(240);
#ifdef DEBUG
  Serial.print("CPU Frequency: ");
  Serial.print(getCpuFrequencyMhz());
  Serial.println(" MHz");
  // esp_log_level_set("*", ESP_LOG_VERBOSE);
  // esp_log_level_set("zh_vector", ESP_LOG_VERBOSE);
  // esp_log_level_set("zh_network", ESP_LOG_VERBOSE);
#else
  esp_log_level_set("*", ESP_LOG_NONE);
#endif

  nvs_flash_init();
  esp_netif_init();
  esp_event_loop_create_default();
  wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&wifi_init_config);
  esp_wifi_set_mode(WIFI_MODE_STA);
  esp_wifi_start();
  // esp_wifi_set_max_tx_power(8); // Power reduction is for example and testing purposes only. Do not use in your own programs!

  uint8_t mac[6];
  esp_wifi_get_mac(WIFI_IF_STA, mac);
#ifdef DEBUG
  printf("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n", MAC2STR(mac));
#endif

  zh_network_init_config_t network_init_config = ZH_NETWORK_INIT_CONFIG_DEFAULT();
  zh_network_init(&network_init_config);

#ifdef PERF
  init_perf();
  return;
#else
#ifdef CONFIG_IDF_TARGET_ESP8266
  esp_event_handler_register(ZH_NETWORK, ESP_EVENT_ANY_ID, &zh_network_event_handler, NULL);
#else
  esp_event_handler_instance_register(ZH_NETWORK, ESP_EVENT_ANY_ID, &zh_network_event_handler, NULL, NULL);
#endif
#endif

#ifndef STATIC
  sensorSetup();
  // xTaskCreatePinnedToCore(
  //     sendIteration,       // Function to run
  //     "sendIterationTask", // Name of the task
  //     4096,                // Stack size in bytes
  //     NULL,                // Parameter
  //     1,                   // Priority
  //     NULL,                // Task handle
  //     1                    // Core to run the task on (0 or 1)
  // // );
  xTaskCreatePinnedToCore(
      requestTimeSync,   // Function to run
      "requestTimeTask", // Name of the task
      4096,              // Stack size in bytes
      NULL,              // Parameter
      1,                 // Priority
      NULL,              // Task handle
      1                  // Core to run the task on (0 or 1)
  );
  // xTaskCreatePinnedToCore(
  //     sensorLoopTask,   // Function to run
  //     "sensorLoopTask", // Name of the task
  //     8192,             // Stack size in bytes
  //     NULL,             // Parameter
  //     1,                // Priority
  //     NULL,             // Task handle
  //     0                 // Core to run the task on (0 or 1)
  // );
#else
  const char *customName = "StaticNode_201"; // Replace with your desired device name
  BLEDevice::init(customName);               // Initialize the BLE device with the custom name
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->start();
#endif
}

void loop()
{
#ifdef PERF
  return;
#endif
#ifndef STATIC
  sensorLoopTask();
#endif
#ifdef ROOT_NODE
  processSerial();
#endif
  delay(10);
}
int processed = 0;

void zh_network_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  // Serial.println(event_id);
  switch (event_id)
  {
  case ZH_NETWORK_ON_RECV_EVENT:
  {
    unsigned long incoming = micros() - offset;
    zh_network_event_on_recv_t *recv_data = (zh_network_event_on_recv_t *)event_data;
    // printf("Message from MAC %02X:%02X:%02X:%02X:%02X:%02X is received. Data lenght %d bytes.\n", MAC2STR(recv_data->mac_addr), recv_data->data_len);
    message_t *recv_message = (message_t *)recv_data->data;
    // Serial.printf("Message type: %d\n", recv_message->message_header.type);
    switch (recv_message->message_header.type)
    {
    case SYNC_REQUEST:
    {
      if (!timeIsSynced)
      {
        return;
      }
#if DEBUG
      printf("Incoming time synchronization request.\nCurrent time: \n");
      printEpochAsDateTime(rtc.getEpoch(), rtc.getMicros());
#endif

      message_t send_message;
      send_message.message_header = {
          .type = SYNC_RESPONSE,
          .timestamp = rtc.getEpoch(),
          .timestamp_us = rtc.getMicros()};
      send_message.sync_response = {
          .t1 = recv_message->sync_request.t1,
          .t1_us = recv_message->sync_request.t1_us,
          .t2 = recv_message->sync_request.t2,       // set by zh_network
          .t2_us = recv_message->sync_request.t2_us, // set by zh_network
                                                     // .server_time_outgoing = micros()                                 // Set by zh network
      };
      zh_network_send(recv_data->mac_addr, (uint8_t *)&send_message, sizeof(send_message));
      break;
    }
    case SYNC_RESPONSE:
    {
      unsigned long t1 = recv_message->sync_response.t1;
      unsigned long t4 = recv_message->sync_response.t4;
      unsigned long t3 = recv_message->sync_response.t3;
      unsigned long t2 = recv_message->sync_response.t2;
      // printf("t1: %d\nt2: %d\nt3: %d\nt4: %d\n", t1, t2, t3, t4);
      unsigned long round_trip_delay = t4 - t1 - (t3 - t2);
      double t1t2 = 0;
      if (t2 > t1)
      {
        t1t2 = ((double)(t2 - t1)) / 2;
      }
      else
      {
        t1t2 = -((double)(t1 - t2)) / 2;
      }

      double t3t4 = 0;
      if (t3 > t4)
      {
        t3t4 = (double)(t3 - t4) / 2;
      }
      else
      {
        t3t4 = -((double)(t4 - t3) / 2);
      }

      long offset = (long)round((t1t2 + t3t4));

      unsigned long t1_us = recv_message->sync_response.t1_us;
      unsigned long t4_us = recv_message->sync_response.t4_us;
      unsigned long t3_us = recv_message->sync_response.t3_us;
      unsigned long t2_us = recv_message->sync_response.t2_us;
      unsigned long round_trip_delay_us = t4_us - t1_us - (t3_us - t2_us);
      long t1t2_us = 0;
      if (t2_us > t1_us)
      {
        t1t2_us = (t2_us - t1_us) / 2;
      }
      else
      {
        t1t2_us = -((t1_us - t2_us) / 2);
      }

      long t3t4_us = 0;
      if (t3_us > t4_us)
      {
        t3t4_us = (t3_us - t4_us) / 2;
      }
      else
      {
        t3t4_us = -((t4_us - t3_us) / 2);
      }

      long offset_us = (t1t2_us + t3t4_us);
      unsigned long time = rtc.getEpoch();
      unsigned long micros = rtc.getMicros();
      rtc.setTime(time + offset, micros + offset_us);
#if DEBUG
      // rtc.setOffset(offset);
      printf("Client time: ");
      printEpochAsDateTime(t1, t1_us);

      printf("Server time: ");
      printEpochAsDateTime(t2, t2_us);

      printf("Server time outgoing:", t3);
      printEpochAsDateTime(t3, t3_us);

      printf("Current time: ");
      printEpochAsDateTime(t4, t4_us);

      // printf("Round trip time: %d s\n", round_trip_delay);
      printf("Offset: %d %d s\n", offset, offset_us);
      // printEpochAsDateTime(time - offset2);

      // printf("Server time: %d\n", server_time);
      printf("Local time: %d\n", rtc.getEpoch());
#endif
      timeIsSynced = true;
      break;
    }
    case MESSAGE:
    {
      break;
    }
    case DATA:
    {
#ifdef ROOT_NODE
      enqueueMessage(*recv_message);
#endif
#ifdef DEBUG
      uint8_t message[sizeof(message_t)];

      dequeueMessage(*reinterpret_cast<message_t *>(&message));

      printf("Incoming data message.\n");
      int test = sizeof(recv_message);
      printf("Id: %d\n", recv_message->message_header.id);
      printf("Message time: ");
      printEpochAsDateTime(recv_message->message_header.timestamp, recv_message->message_header.timestamp_us);

      printf("Data: %d\n", recv_message->data.microphoneData.avgDb);
      printf("Data: %d\n", recv_message->data.microphoneData.peakFrequency);
      printf("Data: %d\n", recv_message->data.microphoneData.zeroCrossingsCount);

      printf("Data: %f\n", recv_message->data.accelerometerData.roll);
      printf("Data: %f\n", recv_message->data.accelerometerData.pitch);
      printf("Data: %f\n", recv_message->data.accelerometerData.yaw);

      printf("Data: [");
      for (int i = 0; i < 10; ++i)
      {
        printf("%d", recv_message->data.bleData[i].deviceName);
        if (i < 9)
          printf(", ");
      }
      printf("], [");
      for (int i = 0; i < 10; ++i)
      {
        printf("%d", recv_message->data.bleData[i].rssi);
        if (i < 9)
          printf(", ");
      }
      printf("]\n");
      for (int i = 0; i < recv_data->data_len; i++)
      {
        Serial.printf("%02X ", ((uint8_t *)recv_message)[i]);
      }
      Serial.println();
      for (int i = 0; i < sizeof(message_t); i++)
      {
        Serial.printf("%02X ", message[i]);
      }
      Serial.println();
#endif
    }
    case RESET_TIME:
    {
      timeIsSynced = false;
      break;
    }
    }
    heap_caps_free(recv_data->data); // Do not delete to avoid memory leaks!
    break;
  }
  case ZH_NETWORK_ON_SEND_EVENT:
  {
    zh_network_event_on_send_t *send_data = (zh_network_event_on_send_t *)event_data;
    if (send_data->status == ZH_NETWORK_SEND_SUCCESS)
    {
    }
    else
    {
#ifdef DEBUG
      printf("Message to MAC %02X:%02X:%02X:%02X:%02X:%02X sent fail.\n", MAC2STR(send_data->mac_addr));
#endif
    }
    break;
  }
  default:
    break;
  }
}