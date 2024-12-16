#ifndef PROXIMITY_H
#define PROXIMITY_H
// #include <NimBLEDevice.h>
#include <Arduino.h>
#include <globals.h>
#include "data_packaging.h"

#include <Wire.h>

void init_proximity();
void getTopDevices(DeviceRSSI device[10]);

#endif