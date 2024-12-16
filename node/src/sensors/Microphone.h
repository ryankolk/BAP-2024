#ifndef MICROPHONE_H
#define MICROPHONE_H
#include <Arduino.h>
#include <driver/i2s.h>
#include <arduinoFFT.h>

bool getMicrophoneData(double &avgDb, double &peakestFrequency, int &zeroCrossingsCount);
void setupMicrophone();
void microphoneTask(void *param);
#endif