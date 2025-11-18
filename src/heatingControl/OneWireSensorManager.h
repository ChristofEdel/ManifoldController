#ifndef __ONEWIRE_SENSOR_MANAGER_H
#define __ONEWIRE_SENSOR_MANAGER_H

#include <string.h>
#include "owb.h"
#include "owb_rmt.h"
#include "ds18b20.h"
#include "SensorManager.h"

struct OneWireSensor : public Sensor {
  private:
    OneWireBus_ROMCode address;    // The actual 8-byte device address
  public:
    OneWireBus_ROMCode & getAddress() { return this->address; }
    DS18B20_Info ds18b20_info;     // DS18B20 device info structure
    void setAddress(const OneWireBus_ROMCode &addr) { 
      this->address = addr; 
      deviceAddressToString(this->address, this->id);
    }
    void clear() {
      memset(&this->address, 0, sizeof(this->address));
      memset(&this->ds18b20_info, 0, sizeof(this->ds18b20_info));
      this->temperature = SensorManager::INVALID_READING;
      this->calibrationOffset = 0.0;
      this->calibrationFactor = 1.0;
      this->id[0] = '\0';
    }
  private: 
    void deviceAddressToString(const OneWireBus_ROMCode &deviceAddress, char *result);
};


// OneWireManager is a class giving access to one OneWireTemperatureSensor struct 
// for every sensor. The constructor takes the I/O pin for the OneWire bus as parameter
class OneWireManager: public SensorManager {
  private:
    static const int maxCount = 20;               // Maximum number of sensors we can handle
    owb_rmt_driver_info m_owbDriverInfo;          // OWB RMT driver info (required for OWB initialization)
    OneWireBus *m_oneWireBus;                     // the OneWire bus used to access the sensors
    OneWireSensor m_sensors[maxCount];            // Array with all the sensors and their data
    int m_count = 0;
    bool sensorPresent(OneWireBus_ROMCode &address);
  public:
    int getCount() { return m_count; }
    inline OneWireSensor & operator[](int index) { return m_sensors[index]; } 
    void setup(int oneWirePin);                    // Inisialise the onewire interface with the given I/O pin; includes scanning for sensors
    void clearSensors();                           // Clear all sensor data 
    void scanForSensors();                         // Scan the onewire bus for devices and add any missing devices to the sensor list
    void readSensor (int index);                   // Read a particular sensor. Access the sensor using the [] operator or using getTemperature()
    void readAllSensors(void);                     // Read all sensors. Access the sensor using the the [] operator or using  getTemperature()

    // Implementation of SensorManager
    virtual Sensor * getSensor(const char *id);
};


#endif