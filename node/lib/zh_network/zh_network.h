#pragma once

#include "string.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "zh_vector.h"
#ifdef CONFIG_IDF_TARGET_ESP8266
#include "esp_system.h"
#else
#include "esp_random.h"
#include "esp_mac.h"
#endif
#include "globals.h"
#include "data_packaging.h"

#define ZH_NETWORK_MAX_MESSAGE_SIZE 91 // Maximum value of the transmitted data size. @attention All devices on the network must have the same ZH_NETWORK_MAX_MESSAGE_SIZE.

#define ZH_NETWORK_INIT_CONFIG_DEFAULT() \
  {                                      \
      .network_id = 0xFAFBFCFD,          \
      .task_priority = 10,               \
      .stack_size = 3072,                \
      .queue_size = 250,                 \
      .max_waiting_time = 1000,          \
      .id_vector_size = 100,             \
      .route_vector_size = 100,          \
      .wifi_interface = WIFI_IF_STA,     \
      .wifi_channel = 1,                 \
      .attempts = 3}

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct // Structure for initial initialization of ESP-NOW interface.
  {
    uint32_t network_id;             // A unique ID for the mesh network. @attention The ID must be the same for all nodes in the network.
    uint8_t task_priority;           // Task priority for the ESP-NOW messages processing. @note It is not recommended to set a value less than 4.
    uint16_t stack_size;             // Stack size for task for the ESP-NOW messages processing. @note The minimum size is 3072 bytes.
    uint8_t queue_size;              // Queue size for task for the ESP-NOW messages processing. @note The size depends on the number of messages to be processed. It is not recommended to set the value less than 32.
    uint16_t max_waiting_time;       // Maximum time to wait a response message from target node (in milliseconds). @note If a response message from the target node is not received within this time, the status of the sent message will be "sent fail".
    uint16_t id_vector_size;         // Maximum size of unique ID of received messages. @note If the size is exceeded, the first value will be deleted. Minimum recommended value: number of planned nodes in the network + 10%.
    uint16_t route_vector_size;      // The maximum size of the routing table. @note If the size is exceeded, the first route will be deleted. Minimum recommended value: number of planned nodes in the network + 10%.
    wifi_interface_t wifi_interface; // WiFi interface (STA or AP) used for ESP-NOW operation. @note The MAC address of the device depends on the selected WiFi interface.
    uint8_t wifi_channel;            // Wi-Fi channel uses to send/receive ESPNOW data. @note Values from 1 to 14.
    uint8_t attempts;                // Maximum number of attempts to send a message. @note It is not recommended to set a value greater than 5.
  } zh_network_init_config_t;

  ESP_EVENT_DECLARE_BASE(ZH_NETWORK);

  typedef enum // Enumeration of possible ESP-NOW events.
  {
    ZH_NETWORK_ON_RECV_EVENT, // The event when the ESP-NOW message was received.
    ZH_NETWORK_ON_SEND_EVENT  // The event when the ESP-NOW message was sent.
  } zh_network_event_type_t;

  typedef enum // Enumeration of possible status of sent ESP-NOW message.
  {
    ZH_NETWORK_SEND_SUCCESS, // If ESP-NOW message was sent success.
    ZH_NETWORK_SEND_FAIL     // If ESP-NOW message was sent fail.
  } zh_network_on_send_event_type_t;

  typedef struct // Structure for sending data to the event handler when an ESP-NOW message was sent. @note Should be used with ZH_NETWORK event base and ZH_NETWORK_ON_SEND_EVENT event.
  {
    uint8_t mac_addr[6];                    // MAC address of the device to which the ESP-NOW message was sent.
    zh_network_on_send_event_type_t status; // Status of sent ESP-NOW message.
    uint8_t *data;                          // Pointer to the data of the received ESP-NOW message.
    uint8_t data_len;                       // Size of the received ESP-NOW message.
  } zh_network_event_on_send_t;

  typedef struct // Structure for sending data to the event handler when an ESP-NOW message was received. @note Should be used with ZH_NETWORK event base and ZH_NETWORK_ON_RECV_EVENT event.
  {
    uint8_t mac_addr[6]; // MAC address of the sender ESP-NOW message.
    uint8_t *data;       // Pointer to the data of the received ESP-NOW message.
    uint8_t data_len;    // Size of the received ESP-NOW message.
  } zh_network_event_on_recv_t;

  /**
   * @brief Initialize ESP-NOW interface.
   *
   * @note Before initialize ESP-NOW interface recommend initialize zh_network_init_config_t structure with default values.
   *
   * @code zh_network_init_config_t config = ZH_NETWORK_INIT_CONFIG_DEFAULT() @endcode
   *
   * @param[in] config Pointer to ESP-NOW initialized configuration structure. Can point to a temporary variable.
   *
   * @return
   *              - ESP_OK if initialization was success
   *              - ESP_ERR_INVALID_ARG if parameter error
   *              - ESP_ERR_WIFI_NOT_INIT if WiFi is not initialized
   *              - ESP_FAIL if any internal error
   */
  esp_err_t zh_network_init(const zh_network_init_config_t *config);

  /**
   * @brief Deinitialize ESP-NOW interface.
   *
   * @return
   *              - ESP_OK if deinitialization was success
   *              - ESP_FAIL if ESP-NOW is not initialized
   */
  esp_err_t zh_network_deinit(void);

  /**
   * @brief Send ESP-NOW data.
   *
   * @param[in] target Pointer to a buffer containing an eight-byte target MAC. Can be NULL for broadcast.
   * @param[in] data Pointer to a buffer containing the data for send.
   * @param[in] data_len Sending data length.
   *
   * @note The function will return an ESP_ERR_INVALID_STATE error if less than 50% of the size set at initialization remains in the message queue.
   *
   * @return
   *              - ESP_OK if sent was success
   *              - ESP_ERR_INVALID_ARG if parameter error
   *              - ESP_ERR_INVALID_STATE if queue for outgoing data is almost full
   *              - ESP_FAIL if ESP-NOW is not initialized or any internal error
   */
  esp_err_t zh_network_send(const uint8_t *target, const uint8_t *data, const uint8_t data_len);

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

  typedef struct
  {
    unsigned long t1;
    unsigned long t1_us;
    unsigned long t2;
    unsigned long t2_us;
  } message_sync_request_t;

  typedef struct
  {
    unsigned long t1;
    unsigned long t1_us;
    unsigned long t2;
    unsigned long t2_us;
    unsigned long t3;
    unsigned long t3_us;
    unsigned long t4;
    unsigned long t4_us;
  } message_sync_response_t;

  typedef struct
  {
    uint32_t value;
    int64_t test;
  } message_generic_t;

  typedef struct
  {
    message_header_t message_header;
    union
    {
      message_sync_request_t sync_request;
      message_sync_response_t sync_response;
      message_generic_t message;
      OutputData data;
      // message_heartbeat heartbeat;
      // uint8_t data[200 - sizeof(message_header)];
    };
  } message_t;

  typedef struct
  {
    message_header_t message_header;
  } message1_t;

  extern int32_t offset;

#ifdef __cplusplus
}
#endif