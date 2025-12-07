#ifndef __STRINGPRINTF_H
#define __STRINGPRINTF_H

#include <Arduino.h>

String StringPrintf(const char* format, ...);
#define BOOL_TO_STRING(x) ((x) ? "true" : "false")

#endif