#include "perf.h"

unsigned long start = 0;
unsigned long end = 0;

uint64_t packets = 0;
uint64_t totalPackets = 0;
uint64_t totalPacketsLost = 0;

// 88:13:BF:09:D7:CC
// 08:F9:E0:B9:EF:FC
uint8_t targetPerf[6] = {0x08, 0xF9, 0xE0, 0xB9, 0xEF, 0xFC};

uint16_t delayTime = 5;

void testPacketsPerSecond(void *pv)
{
  while (true)
  {
    end = millis();
    Serial.print(end - start);
    Serial.print(" ms, ");
    Serial.print(packets);
    Serial.print(" packets, ");
    Serial.print(delayTime);
    Serial.println(" ms delay");
    packets = 0;
    start = millis();
    if (delayTime > 10)
    {
      delayTime -= 10;
    }
    else if (delayTime > 2)
    {
      delayTime -= 1;
    }
    delay(1000);
  }
}

void testReliability(void *pv)
{
  while (true)
  {
    Serial.print(millis());
    Serial.print(" ms, ");
    Serial.print(totalPackets);
    Serial.print(" packets, ");
    Serial.print(totalPacketsLost);
    Serial.println(" lost");
    delay(10000);
  }
}

void stress(void *pv)
{
  while (true)
  {
    message1_t send_message;
    send_message.message_header = {
        .type = MESSAGE};
    // send_message.message = {};
    packets++;
    zh_network_send(targetPerf, (uint8_t *)&send_message, sizeof(send_message));
    delay(delayTime);
  }
}

void measureRTT(void *pv)
{
  while (true)
  {
    message1_t send_message;
    send_message.message_header = {
        .type = MESSAGE,
        .id = 255,
        .timestamp = micros(),
        .timestamp_us = 255};
    // send_message.message = {};
    packets++;
    zh_network_send(targetPerf, (uint8_t *)&send_message, sizeof(send_message));
    delay(delayTime);
  }
}

void zh_network_event_handler_perf(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
  zh_network_event_on_recv_t *recv_data = (zh_network_event_on_recv_t *)event_data;
  message1_t *recv_message = (message1_t *)recv_data->data;
  switch (event_id)
  {
  case ZH_NETWORK_ON_RECV_EVENT:
  {
    switch (recv_message->message_header.type)
    {
    case MESSAGE:
    {

      // Serial.print(" ms, ");
      // Serial.print(totalPackets);
      // Serial.print(" packets, ");
      // Serial.print(totalPacketsLost);
      // Serial.println(" lost");
      packets++;
      totalPackets++;
    }
    }
    heap_caps_free(recv_data->data); // Do not delete to avoid memory leaks!
    break;
  }
  case ZH_NETWORK_ON_SEND_EVENT:
  {
    zh_network_event_on_send_t *send_data = (zh_network_event_on_send_t *)event_data;
    message1_t *recv_message = (message1_t *)send_data->data;
    if (send_data->status == ZH_NETWORK_SEND_SUCCESS)
    {
      // if (totalPackets - totalPacketsLost < 1000)
      // {
      //   // Serial.print(recv_message->message_header.timestamp);
      //   // Serial.print("\t");
      //   Serial.println(micros() - recv_message->message_header.timestamp);
      // }
      totalPackets++;
      // printf("Message to MAC %02X:%02X:%02X:%02X:%02X:%02X sent success.\n", MAC2STR(send_data->mac_addr));
    }
    else
    {
      totalPacketsLost++;
      totalPackets++;
      // printf("Message to MAC %02X:%02X:%02X:%02X:%02X:%02X sent fail.\n", MAC2STR(send_data->mac_addr));
    }
    break;
  }
  default:
    break;
  }
}

void init_perf()
{
  esp_log_level_set("*", ESP_LOG_NONE);
  esp_event_handler_instance_register(ZH_NETWORK, ESP_EVENT_ANY_ID, &zh_network_event_handler_perf, NULL, NULL);

  // xTaskCreate(
  //     testPacketsPerSecond,
  //     "TestPacketsPerSecondTask",
  //     3000,
  //     NULL,
  //     10,
  //     NULL);

  xTaskCreate(
      testReliability,
      "TestReliabilityTask",
      3000,
      NULL,
      10,
      NULL);

#ifdef SENDER
  xTaskCreate(
      measureRTT,
      "StressTask",
      3000,
      NULL,
      10,
      NULL);
#endif
}