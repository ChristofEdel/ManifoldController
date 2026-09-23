#include "MyConfig.h"

#include "NeohubZoneManager.h"
#include "MyLog.h"
#include "SensorMap.h"
#include "Filesystem.h"
#include "MqttManager.h"
#include <limits>


//-------------------------------------------------------------------------------------------------
// The global singleton
//-------------------------------------------------------------------------------------------------

CConfig Config;

namespace {
    bool isNull(double v) { return isnan(v); }
    bool isNull(long v) { return false; }
    bool isNull(bool v) { return false; }
    bool isNull(const String& s) { return false; }

    void setDefault(double& v) { v = std::numeric_limits<double>::quiet_NaN(); }
    void setDefault(long& v) { v = 0; }
    void setDefault(bool& v) { v = false; }
    void setDefault(String& v) { v = ""; }
}

/// @brief Save the configuration parameters to a file on the chip flash memory
void CConfig::save() const
{
    JsonDocument configJson;

    // Main config parameters
    #define CONFIG_SAVE(type, name, pType, displayName)       \
        if (!isNull(_##name))                                 \
            configJson[_toParameterKey(#name)] = _##name;

    CONFIG_PARAMETERS(CONFIG_SAVE)

    #undef CONFIG_SAVE
    
    for (int i = 0; i < SensorMap.getCount(); i++) {
        SensorMapEntry* entry = SensorMap[i];
        configJson["sensors"][i]["id"] = entry->id;
        configJson["sensors"][i]["name"] = entry->name;
    }

    int i = 0;
    for (auto z : NeohubZoneManager.getAllZones()) {
        configJson["zones"][i]["id"] = z.id;
        configJson["zones"][i]["name"] = z.name;
        i++;
    }

    i = 0;
    for (auto z : NeohubZoneManager.getActiveZones()) {
        configJson["activeZones"][i]["id"] = z.id;
        configJson["activeZones"][i]["name"] = z.name;
        i++;
    }

    i = 0;
    for (auto z : NeohubZoneManager.getMonitoredZones()) {
        configJson["monitoredZones"][i]["id"] = z.id;
        configJson["monitoredZones"][i]["name"] = z.name;
        i++;
    }

    // Serialize to SD card
    MyLog.print("Saving configuration...");\
    save(configJson, masterFileName);
    save(configJson, secondaryFileName);

    MyLog.println("done");
}

void CConfig::save(JsonDocument &configJson, const char *fileName) const {
    Filesystem.lock();
    File file = Filesystem.open(fileName, FILE_WRITE);
    if (!file) {
        Filesystem.unlock();
        MyLog.printf("Failed to open config file '%s' for writing\n", fileName);
        return;
    }
    serializeJsonPretty(configJson, file);
    file.close();
    Filesystem.unlock();
    return;
}


/// @brief Load the configuration parameters from a file on the chip flash memory
void CConfig::load()
{
    // Read the file ------------------------------------------
    MyLog.print("Loading configuration...");

    String contents;
    Filesystem.lock();
    File file = Filesystem.open(masterFileName, FILE_READ);
    if (!file) file = Filesystem.open(secondaryFileName, FILE_READ);   // old name / location
    if (!file) {
        Filesystem.unlock();
        MyLog.printf("Failed to open config file %s and %s for reading\n", masterFileName, secondaryFileName);
        this->applyDefaults();
        return;
    }

    contents = file.readString();
    file.close();
    Filesystem.unlock();

    // Interpret the JSON ------------------------------------
    JsonDocument configJson;
    DeserializationError error = deserializeJson(configJson, contents);
    if (error) {
        MyLog.print("Failed to parse config file: ");
        MyLog.println(error.c_str());
        this->applyDefaults();
        return;
    }

    // Set the parameters from the  JSON
    #define CONFIG_LOAD(type, name, pType, displayName)                \
        if (!configJson[_toParameterKey(#name)].isNull())              \
            _##name = configJson[_toParameterKey(#name)].as<type>();   \
        else                                                           \
            setDefault (_##name);

    CONFIG_PARAMETERS(CONFIG_LOAD)

    #undef CONFIG_LOAD
    
    // Transition
    if (configJson["controlModeAsInt"].isNull() && !configJson["controlModeAsInt"].isNull()) {
        _ControlModeAsInt = configJson["controlModeAsInt"].as<long>();
    }

    // Iterate over sensors
    JsonArray sensorsArray = configJson["sensors"].as<JsonArray>();
    SensorMap.clear();
    for (JsonObject sensorObj : sensorsArray) {
        SensorMap.setNameForId(sensorObj["id"].as<String>(), sensorObj["name"].as<String>());
    }

    // Iterate over zones
    JsonArray zones = configJson["zones"].as<JsonArray>();
    NeohubZoneManager.clearZones();
    for (JsonObject zone : zones) {
        NeohubZoneManager.addZone(NeohubZone(zone["id"].as<int>(), zone["name"].as<String>()));
    }

    JsonArray activeZones = configJson["activeZones"].as<JsonArray>();
    NeohubZoneManager.clearActiveZones();
    for (JsonObject zone : activeZones) {
        NeohubZoneManager.addActiveZone(NeohubZone(zone["id"].as<int>(), zone["name"].as<String>()));
    }

    JsonArray monitoredZones = configJson["monitoredZones"].as<JsonArray>();
    NeohubZoneManager.clearMonitoredZones();
    for (JsonObject zone : monitoredZones) {
        NeohubZoneManager.addMonitoredZone(NeohubZone(zone["id"].as<int>(), zone["name"].as<String>()));
    }

    MyLog.println("done");
}

/* static */ String CConfig::_toParameterKey(const String& name)
{
    String result(name);

    if (!result.isEmpty()) {
        result[0] = tolower(static_cast<unsigned char>(result[0]));
    }

    return result;
}

// #endregion
//-------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------
// #region Sending to MQTT
//-------------------------------------------------------------------------------------------------

void CConfig::publishConfigToMqtt(const String& deviceId, int retentionSeconds)
{
    _retentionSeconds = retentionSeconds;
    #define CONFIG_PUBLISH(type, name, pType, displayName)                  \
        {                                                                   \
            String topic = "manifold/" + deviceId +                         \
                        "/config/" + _toParameterKey(#name);                \
            String payload = CMqttManager::makePayload(_##name);            \
                                                                            \
            MqttManager.publishRetainedTopic(                               \
                topic,                                                      \
                payload,                                                    \
                retentionSeconds                                            \
            );                                                              \
        }

    CONFIG_PARAMETERS(CONFIG_PUBLISH)

#undef CONFIG_PUBLISH
}

String CConfig::getHostnameFirstPart() {
    if (this->_Hostname.isEmpty()) return "";
    int dot = this->_Hostname.indexOf('.');
    if (dot >= 0) return this->_Hostname.substring(0, dot);
    return this->_Hostname;
}

bool CConfig::publishAutodiscoverTopics(const String& deviceId)
{
    HomeassistantMqttDiscovery d(
        "manifold", deviceId, this->getHostnameFirstPart(), "Manifold " + this->_Name
    );

    // Main config parameters
    #define CONFIG_AUTOCONFIGURE(type, name, pType, displayName) \
        if (!String(displayName).isEmpty()) {                    \
            this->_publish##pType##Autodiscover(                 \
                d, #name, displayName                            \
            );                                                   \
        }                                                        \

    CONFIG_PARAMETERS(CONFIG_AUTOCONFIGURE)

    #undef CONFIG_AUTOCONFIGURE

    return true;
}

void CConfig::_publishTemperatureAutodiscover(HomeassistantMqttDiscovery &d, const String& name, const String& displayName) {
    d.publishTemperatureParameter("config", _toParameterKey(name), displayName);
}

// #endregion
//-------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------
// #region Updates from MQTT
//-------------------------------------------------------------------------------------------------

void CConfig::subscribeToMqttSetTopics(const String& deviceId)
{
    _deviceId = deviceId;
    MqttManager.subscribeTopic("manifold/" + deviceId + "/config/+/set", _mqttCallback, this );
}

void CConfig::_updateFromMqtt(const String& topic, const String& payload)
{
    String publishTopic = "heating/" + _deviceId + "/config/";
    String publishPayload;

    #define CONFIG_UPDATE(type, name, pType, displayName) \
        if (topic.endsWith("/" + _toParameterKey(#name) + "/set")) {            \
            type value;                                                         \
            if (CMqttManager::parse(payload, value)) {                          \
                this->_##name = value;                                          \
                publishTopic += _toParameterKey(#name);                         \
                publishPayload = CMqttManager::makePayload(_##name);            \
                MqttManager.publishRetainedTopic(                               \
                    publishTopic,                                               \
                    publishPayload,                                             \
                    _retentionSeconds                                           \
                );                                                              \
                MyLog.printf("Config parameter %s set to %s\n", #name, payload);\
                this->save();                                                   \
            }                                                                   \
        }                                                                       \

    CONFIG_PARAMETERS(CONFIG_UPDATE)
}
void CConfig::_mqttCallback(void *arg, const String& topic, const String& payload)
{
    ((CConfig *)arg)->_updateFromMqtt(topic, payload);
}


// #endregion
//-------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------
// #region Initialisation
//-------------------------------------------------------------------------------------------------

void CConfig::applyDefaults()
{
    this->_Hostname = "whatever";
    this->_RoomSetpoint            = 20.0;
    this->_RoomProportionalGain    = 5.0;
    this->_RoomIntegralMinutes     = 180;

    this->_FlowMinSetpoint         = 25.0;
    this->_FlowMaxSetpoint         = 37.0;

    this->_FlowProportionalGain    = 3;
    this->_FlowIntegralSeconds     = 10;
}

// #endregion
//-------------------------------------------------------------------------------------------------