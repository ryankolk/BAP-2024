#ifndef ACCELEROMETER_H
#define ACCELEROMETER_H

#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "globals.h"

void startAccelerometerTask();
void setupAccelerometer();
void getAccelerometerData(float &currentRoll, float &currentPitch, float &currentYaw);
void calibrateAccelerometer();
#endif