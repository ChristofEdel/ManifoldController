#ifndef __ONEWIRE_SENSOR_MANAGER_H
#define __ONEWIRE_SENSOR_MANAGER_H
#include "ds18b20.h"
#include "owb.h"
#include "owb_rmt.h"

struct OneWireSensor {
  public:
    char id[17];        // the ID of this sensor
    float temperature;  // the temperature read from the sensor

    float calibrationOffset = 0.0;  // an offset from the sensor temperature to calculate a calibrated temperature
    float calibrationFactor = 1.0;  // a linear calibration factor

    int readings;          // How often this sensor was read
    int crcErrors;         // how many CRC errors occured
    int noResponseErrors;  // how many times the sensor did not respond
    int otherErrors;       // how many times an other error occurred
    int failures;          // how many times the sensor could still not be read even after retrying

    inline float calibratedTemperature() { return (this->temperature + calibrationOffset) * calibrationFactor; };

  private:
    const OneWireBus* oneWireBus;       // The bus this device is connected to
    OneWireBus_ROMCode oneWireAddress;  // The actual 8-byte device address
    DS18B20_Info ds18b20_info;          // DS18B20 device info structure

  public:
    OneWireSensor(OneWireBus *bus);
    OneWireBus_ROMCode& getOneWireAddress() { return this->oneWireAddress; }
    void setOneWireAddress(const OneWireBus_ROMCode& addr);
    void setId(const char* id);
    void clear();

  private:
    void deviceAddressToString(const OneWireBus_ROMCode& deviceAddress, char* result);
    void stringToDeviceAddress(const char* result, OneWireBus_ROMCode& deviceAddress);

    friend class OneWireManager;
};

// OneWireManager is a class giving access to one OneWireTemperatureSensor struct
// for every sensor. The constructor takes the I/O pin for the OneWire bus as parameter
class OneWireManager {
  private:
    static const int maxCount = 20;       // Maximum number of sensors we can handle
    owb_rmt_driver_info m_owbDriverInfo;  // OWB RMT driver info (required for OWB initialization)
    OneWireBus* m_oneWireBus;             // the OneWire bus used to access the sensors
    OneWireSensor *m_sensors[maxCount];   // Array with all the sensors and their data
    int m_count = 0;
    bool sensorPresent(OneWireBus_ROMCode& address);

  public:
    int getCount() { return m_count; }
    inline OneWireSensor& operator[](int index) { return *m_sensors[index]; }

    void setup(int oneWirePin);           // Inisialise the onewire interface with the given I/O pin
    void scanForSensors();                // Scan the onewire bus for devices and add any missing devices to the sensor list
    void addKnownSensor(const char* id);  // Add a known sensor without reading its information from the bus
    void clearSensors();                  // Clear all sensor data

    void readAllSensors(void);                       // Read and store the temperature from all known sensors
    OneWireSensor* getSensor(const char* id);        // Get the data for the sensor with the given ID
    float getTemperature(const char* id);            // Get the last read uncalibrated temperature for the sensor with the given id
    float getCalibratedTemperature(const char* id);  // Get the last read calibrated temperature for the sensor with the given id
    static const int SENSOR_NOT_FOUND = -300;
    static const int INVALID_READING = -200;
};

#endif