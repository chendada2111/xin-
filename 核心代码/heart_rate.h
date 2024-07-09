#ifndef HEART_RATE_H
#define HEART_RATE_H

uint16_t GetVoltage(void);
static uint16_t MaxElement(uint16_t data[]);
static uint16_t MinElement(uint16_t data[]);
uint16_t Rate_Calculate(uint16_t data);
uint16_t heartTask(void);

#endif