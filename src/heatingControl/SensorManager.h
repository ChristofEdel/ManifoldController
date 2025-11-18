#ifndef __SENSOR_MANAGER_H
#define __SENSOR_MANAGER_H

struct Sensor {
  char id[17];                         
  float temperature;
  float calibrationOffset;
  float calibrationFactor;
  int errorCode;
  int readings;
  int crcErrors;
  int noResponseErrors;
  int failures;
  inline float calibratedTemperature() { return  (this->temperature + calibrationOffset) * calibrationFactor; };
  Sensor() { this->calibrationOffset = 0.0; this->calibrationFactor = 1.0; }
};

class SensorManager {
  public:
    virtual Sensor * getSensor(const char *id) = 0;
    virtual float getTemperature(const char *id) { Sensor *s = getSensor(id); if (!s) return SENSOR_NOT_FOUND; return s->temperature; };
    virtual float getCalibratedTemperature(const char *id) { Sensor *s = getSensor(id); if (!s) return SENSOR_NOT_FOUND; return s->calibratedTemperature(); };
    static const int SENSOR_NOT_FOUND = -300;
    static const int INVALID_READING = -200;
};

#endif