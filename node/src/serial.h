#ifndef SERIAL_H
#define SERIAL_H
#include <Arduino.h>
#include "zh_network.h"
// typedef struct
// {
//   uint32_t value1;
//   uint32_t value2;
//   uint32_t value3;
// } message_type1_t;

bool enqueueMessage(const message_t &message);

uint16_t checksumCalculator(uint8_t *data, uint16_t length);
void processSerial();

bool dequeueMessage(message_t &message);

#endif