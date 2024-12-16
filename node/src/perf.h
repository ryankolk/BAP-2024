#ifndef PERF_H
#define PERF_H

#include "nvs_flash.h"
#include "esp_netif.h"
#include "zh_network.h"
#include "Arduino.h"

// #define SENDER

extern unsigned long start;
extern unsigned long end;

extern uint64_t packets;
extern uint64_t totalPackets;
extern uint64_t totalPacketsLost;

extern uint8_t targetPerf[6];

extern uint16_t delayTime;

void testPacketsPerSecond(void *pv);
void testReliability(void *pv);
void stress(void *pv);
void zh_network_event_handler_perf(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
void init_perf();

#endif // PERF_H