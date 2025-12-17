#include "OneWireManager.h"

#include <Arduino.h>

#include "MyLog.h"

// Define the global singleton
COneWireManager OneWireManager;

///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// OneWireSensor
//

OneWireSensor::OneWireSensor(OneWireBus* bus)
{
    this->oneWireBus = bus;
    this->ds18b20_info.init = false;

    this->readings = 0;        
    this->crcErrors = 0;       
    this->noResponseErrors = 0;
    this->otherErrors = 0;     
    this->failures = 0;        

};

// Set the address of this sensor on the OneWireBus
// Updates the id accordingly
void OneWireSensor::setOneWireAddress(const OneWireBus_ROMCode& addr)
{
    this->oneWireAddress = addr;
    this->ds18b20_info.rom_code = addr;
    deviceAddressToString(this->oneWireAddress, this->id);

    // Initialise the ds18b20 info to its standard values
    // so we can use the ds18b20 lib without having to talk
    // to the sensor first
    this->ds18b20_info.init = true;
    this->ds18b20_info.solo = false;
    this->ds18b20_info.use_crc = true;
    this->ds18b20_info.bus = this->oneWireBus;
    this->ds18b20_info.resolution = DS18B20_RESOLUTION_12_BIT;

    this->readings = 0;        
    this->crcErrors = 0;       
    this->noResponseErrors = 0;
    this->otherErrors = 0;     
    this->failures = 0;     
}

// Set the id of this sensor
// Updates the onewire address accordingly
void OneWireSensor::setId(const char* id)
{
    strncpy(this->id, id, sizeof(this->id));
    stringToDeviceAddress(id, this->oneWireAddress);
    ds18b20_info.rom_code = this->oneWireAddress;

    this->ds18b20_info.init = true;
    this->ds18b20_info.solo = false;
    this->ds18b20_info.use_crc = true;
    this->ds18b20_info.bus = this->oneWireBus;
    this->ds18b20_info.resolution = DS18B20_RESOLUTION_12_BIT;

    
    this->readings = 0;        
    this->crcErrors = 0;       
    this->noResponseErrors = 0;
    this->otherErrors = 0;     
    this->failures = 0;     
}

// Clear all sensor information
void OneWireSensor::clear()
{
    memset(&this->oneWireAddress, 0, sizeof(this->oneWireAddress));
    memset(&this->ds18b20_info, 0, sizeof(this->ds18b20_info));
    
    this->lastUpdate = 0;
    this->temperature = COneWireManager::INVALID_READING;
    this->calibrationOffset = 0.0;
    this->calibrationFactor = 1.0;
    this->id[0] = '\0';

    this->readings = 0;        
    this->crcErrors = 0;       
    this->noResponseErrors = 0;
    this->otherErrors = 0;     
    this->failures = 0;     
}

// Convert a device address ("ROM CODE") to its hexadecimal string equivalent
void OneWireSensor::deviceAddressToString(const OneWireBus_ROMCode& deviceAddress, char* result)
{
    result[16] = '\0';
    for (uint8_t i = 0; i < 8; i++) {
        sprintf(&result[i * 2], "%02X", deviceAddress.bytes[i]);
    }
}

// Convert a hexadecimal string to the device address ("ROM CODE")
void OneWireSensor::stringToDeviceAddress(const char* id, OneWireBus_ROMCode& result)
{
    // If the ID is not a 8-byte hex string, clear the result and give up
    if (strlen(id) != 16) {
        for (int i = 0; i < 8; i++) result.bytes[i] = 0;
    }
    // Decode the 16-character hex string into an 8-byte array
    char hexByte[3];
    hexByte[3] = '\0';
    const char* cp = id;
    for (int i = 0; i < 8; i++) {
        hexByte[0] = *cp++;
        hexByte[1] = *cp++;
        this->oneWireAddress.bytes[i] = (uint8_t)strtol(hexByte, nullptr, 16);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// OneWireManager
//

// Initialise this OneWireManager to use the bus on the given pin
void COneWireManager::setup(int pin)
{
    // Initialise the interface
    this->m_oneWireBus = owb_rmt_initialize(
        &this->m_owbDriverInfo,
        (gpio_num_t)pin,
        (rmt_channel_t)0,  // The RMT channel to use for transmit
        (rmt_channel_t)4   // The RMT channel to use for receive
    );

    for (int i = 0; i < maxCount; i++) {
        this->m_sensors[i] = new OneWireSensor(this->m_oneWireBus);
    }

    // Turn on CRC checking
    owb_use_crc(this->m_oneWireBus, true);

    // this->m_oneWireBus->logStream = &Serial;
}

// Clear the list of sensors we know about
void COneWireManager::clearSensors()
{
    for (int i = 0; i < maxCount; i++) {
        this->m_sensors[i]->clear();
    }
    this->m_count = 0;
}

// Scan for sensors to the bus and add them to our sensor list
void COneWireManager::scanForSensors()
{
    // Stop if there is no space for more sensors
    if (this->m_count == maxCount) return;

    // Scan the bus for all sensors available on the bus
    OneWireBus_SearchState search_state = {};
    int sensorIndex = this->m_count;

    bool found;
    owb_search_first(this->m_oneWireBus, &search_state, &found);

    while (found) {
        if (
            search_state.rom_code.fields.family[0] == 0x28  // Only DS18B20 sensors
            && !this->sensorPresent(search_state.rom_code)  // Only sensors we don't already know about
        ) {
            // Store sensor data in our array
            OneWireSensor* sensorData = m_sensors[sensorIndex];
            sensorData->clear();
            sensorData->setOneWireAddress(search_state.rom_code);  // also sets Id
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
        OneWireSensor* sensorData = m_sensors[sensorIndex];
        // Initialize DS18B20 info structure
        ds18b20_init(&sensorData->ds18b20_info, this->m_oneWireBus, sensorData->getOneWireAddress());
        ds18b20_use_crc(&sensorData->ds18b20_info, true);

        // Set the resolution to 12 bit if not already set
        if (sensorData->ds18b20_info.resolution != DS18B20_RESOLUTION_12_BIT) {
            ds18b20_set_resolution(&sensorData->ds18b20_info, DS18B20_RESOLUTION_12_BIT);
        }
    }

    // Clear the data for all remaining sensor entries
    while (sensorIndex < maxCount) {
        OneWireSensor* sensorData = this->m_sensors[sensorIndex];
        sensorData->clear();
        sensorIndex++;
    }

    return;
}

// Add a known sensor without reading its information from the bus
void COneWireManager::addKnownSensor(const char* id)
{
    if (getSensor(id)) return;  // already present
    OneWireSensor* sensorData = m_sensors[this->m_count++];
    sensorData->setId(id);
}

// Check if we already have an entry for the sensor with the given address
bool COneWireManager::sensorPresent(OneWireBus_ROMCode& address)
{
    for (int i = 0; i < this->m_count; i++) {
        OneWireBus_ROMCode& addr = this->m_sensors[i]->getOneWireAddress();
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

// Read ands store the temperature from all known sensors
void COneWireManager::readAllSensors()
{
    if (this->m_count == 0) return;
    ds18b20_convert_all(this->m_oneWireBus);
    ds18b20_wait_for_conversion_r(this->m_oneWireBus, DS18B20_RESOLUTION_12_BIT);

    for (int i = 0; i < this->m_count; i++) {
        OneWireSensor* si = this->m_sensors[i];

        // Try up to 3 times to read the temperature
        int attemptsLeft = 3;
        DS18B20_ERROR result = DS18B20_ERROR_UNKNOWN;
        while (attemptsLeft--) {
            result = ds18b20_read_temp(&si->ds18b20_info, &si->temperature);
            si->readings++;
            if (result == DS18B20_OK) {
                si->lastUpdate = time(nullptr);
                break;
            }

            // Count specific error types
            if (result == DS18B20_ERROR_CRC) {
                si->crcErrors++;
            }
            else if (result == DS18B20_ERROR_NO_DATA) {
                si->noResponseErrors++;
            }
            else {
                si->otherErrors++;
                break;  // no retries for these, this makes it worse
            }
        }

        // If none of the attempts were successful, report an invalid reading
        if (result != DS18B20_OK) {
            si->temperature = COneWireManager::INVALID_READING;
            si->failures++;
        }
    }
    return;
}

// Get the data for the sensor with the given ID
OneWireSensor* COneWireManager::getSensor(const char* id)
{
    for (int i = 0; i < this->m_count; i++) {
        if (strcmp(id, this->m_sensors[i]->id) == 0) return this->m_sensors[i];
    }
    return nullptr;
}

// Get the last read temperature for the sensor with the given id
float COneWireManager::getTemperature(const char* id)
{
    OneWireSensor* s = getSensor(id);
    if (!s) return SENSOR_NOT_FOUND;
    return s->temperature;
};

// Get the last calibrated temperature for the sensor with the given id
float COneWireManager::getCalibratedTemperature(const char* id)
{
    OneWireSensor* s = getSensor(id);
    if (!s) return SENSOR_NOT_FOUND;
    return s->calibratedTemperature();
};
