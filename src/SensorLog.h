#ifndef __SENSOR_LOG_H
#define __SENSOR_LOG_H

#include <Arduino.h>
void logSensors();
String getSensorHeaderLine();
String getSensorLogLine();
void logSensorIssues();

#endif
