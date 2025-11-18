#include "OneWireSensorManager.h"
#include "MyLog.h"
#include <Arduino.h>

void OneWireManager::setup(int pin) {
  this->m_oneWireBus = owb_rmt_initialize(
    &this->m_owbDriverInfo, 
    (gpio_num_t) pin,
    (rmt_channel_t) 0, 
    (rmt_channel_t) 4
  );
  // this->m_oneWireBus->logStream = &Serial; // add detailed logging
  owb_use_crc(this->m_oneWireBus, true);
  this->scanForSensors();
}


void OneWireManager::clearSensors() {
  for (int i = 0; i < maxCount; i++) {
    this->m_sensors[i].clear();
  }
  this->m_count = 0;
}

void OneWireManager::scanForSensors() {

  // Stop if there is no space for more sensors
  if (this->m_count == maxCount) return;

  // Scan the bus for all sensors available on the bus
  OneWireBus_SearchState search_state = {};
  int sensorIndex = this->m_count;

  bool found;
  owb_search_first(this->m_oneWireBus, &search_state, &found); 

  while (found) {

    if (
      search_state.rom_code.fields.family[0] == 0x28    // Only DS18B20 sensors
      && !this->sensorPresent(search_state.rom_code)    // Only sensors we don't already know about
    )
    {
      // Store sensor data in our array 
      OneWireSensor *sensorData = &m_sensors[sensorIndex];
      sensorData->clear();
      sensorData->setAddress(search_state.rom_code);
      sensorIndex++;
    }

    // Stop if there is no space for more sensors or if this was the last one on the bus
    if (sensorIndex >= maxCount) break;
    if (search_state.last_device_flag) break;

    // Scan next sensor
    if (owb_search_next(this->m_oneWireBus, &search_state, &found) != OWB_STATUS_OK) break;
  }
  this->m_count = sensorIndex;

  // Once we have found all sensors, initialize them
  // We also initialise any sensors that were already present
  for (sensorIndex = 0; sensorIndex < this->m_count; sensorIndex++) {
    OneWireSensor *sensorData = &m_sensors[sensorIndex];
    // Initialize DS18B20 info structure
    ds18b20_init(&sensorData->ds18b20_info, this->m_oneWireBus, sensorData->getAddress());
    ds18b20_use_crc(&sensorData->ds18b20_info, true);

    // Set the resolution to 12 bit if not already set
    if (sensorData->ds18b20_info.resolution != DS18B20_RESOLUTION_12_BIT) {
      ds18b20_set_resolution(&sensorData->ds18b20_info, DS18B20_RESOLUTION_12_BIT);
    }
  }

  // Clear the data for all remaining sensor entries
  while (sensorIndex < maxCount) {
    OneWireSensor * sensorData = &this->m_sensors[sensorIndex];
    sensorData->clear();
    sensorIndex++;
  }

  return;
}

bool OneWireManager::sensorPresent(OneWireBus_ROMCode &address) {
  for (int i = 0; i < this->m_count; i++) {
    OneWireBus_ROMCode &addr = this->m_sensors[i].getAddress();
    bool match = true;
    for (int b = 0; b < 8; b++) {
      if (addr.bytes[b] != address.bytes[b]) {
        match = false;
        break;
      }
    }
    if (match) return true;
  }
  return false;
}

// void OneWireManager::readSensor(int index)
// {
//   if (index < 0 || index >= this->m_count) return;
//   OneWireSensor *sensor = m_sensors[index];
//   sensor->temperature = m_dallasTemperature->getTempC(sensor->address);
//   if (sensor->temperature == DEVICE_DISCONNECTED_C) sensor->temperature = SensorManager::INVALID_READING;
// }

void OneWireManager::readAllSensors()
{
  if (this->m_count == 0) return;
  ds18b20_convert_all(this->m_oneWireBus);
  ds18b20_wait_for_conversion(&this->m_sensors[0].ds18b20_info);

  for (int i = 0; i < this->m_count; i++) {
    OneWireSensor *si = &this->m_sensors[i];

    // Try up to 3 times to read the temperature
    int attemptsLeft = 3;
    DS18B20_ERROR result = DS18B20_ERROR_UNKNOWN;
    while (attemptsLeft--) {

      result = ds18b20_read_temp(&si->ds18b20_info, &si->temperature);
      si->readings++;
      if (result == DS18B20_OK) break;

      // Count specific error types
      if (result == DS18B20_ERROR_CRC) {
        si->crcErrors++;
      } 
      else if (result == DS18B20_ERROR_NO_DATA) {
        si->noResponseErrors++;
      }
      else {
        break;  // don't retry for other error types
      }
    }

    // If none of the attempts were successful, set error code and invalid reading
    if (result != DS18B20_OK) {
      si->temperature = SensorManager::INVALID_READING;
      si->errorCode = result;
      si->failures++;
    }
    // Otherwise clear error code
    else {
      si->errorCode = 0;
    }
  }
  return;
}

void OneWireSensor::deviceAddressToString(const OneWireBus_ROMCode &deviceAddress, char *result) {
  result[16] = '\0';
  for (uint8_t i = 0; i < 8; i++) {
    sprintf(&result[i*2], "%02X", deviceAddress.bytes[i]);
  }
}

Sensor * OneWireManager::getSensor(const char *id) {
  for (int i = 0; i < this->m_count; i++) {
    if (strcmp(id, this->m_sensors[i].id) == 0) return &this->m_sensors[i];
  }
  return nullptr;
}




