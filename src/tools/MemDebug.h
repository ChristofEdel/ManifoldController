#include <Arduino.h>
#include <esp_system.h>
#ifndef __MEMDEBUG_H

#define __MEMDEBUG_H

void setupMemDebug();
void checkMemoryChange(int thresholdBytes = 1024);
void checkMemoryChange(bool forceOutput, int thresholdBytes = 1024);
void checkMemoryChange(char *message, bool forceOutput = true, int thresholdBytes = 1024);
void outOfMemory();

#endif