#include "MyConfig.h"

#include "MyLog.h"

CConfig Config;

void CConfig::saveToSdCard(SdFs& fs, MyMutex& fsMutex, const String& filename, const SensorMap& sensorMap, CNeohubManager& neohub) const
{
    JsonDocument configJson;

    configJson["hostname"]                 = hostname;
    configJson["neohubAddress"]            = neohubAddress;
    configJson["neohubToken"]              = neohubToken;
    configJson["heatingControllerAddress"] = heatingControllerAddress;

    configJson["flowMaxSetpoint"]          = flowMaxSetpoint;
    configJson["flowMinSetpoint"]          = flowMinSetpoint;

    configJson["flowSensorId"]             = flowSensorId;
    configJson["inputSensorId"]            = inputSensorId;
    configJson["returnSensorId"]           = returnSensorId;

    configJson["flowProportionalGain"]     = flowProportionalGain;
    configJson["flowIntegralSeconds"]      = flowIntegralSeconds;  
    configJson["flowValveInverted"]        = flowValveInverted;

    configJson["roomSetpoint"]             = roomSetpoint;
    configJson["roomProportionalGain"]     = roomProportionalGain;  
    configJson["roomIntegralMinutes"]      = roomIntegralMinutes;

    for (int i = 0; i < sensorMap.getCount(); i++) {
        SensorMapEntry* entry = sensorMap[i];
        configJson["sensors"][i]["id"] = entry->id;
        configJson["sensors"][i]["name"] = entry->name;
    }

    int i = 0;
    for (auto z : neohub.getActiveZones()) {
        configJson["activeZones"][i]["id"] = z.id;
        configJson["activeZones"][i]["name"] = z.name;
        i++;
    }

    i = 0;
    for (auto z : neohub.getMonitoredZones()) {
        configJson["monitoredZones"][i]["id"] = z.id;
        configJson["monitoredZones"][i]["name"] = z.name;
        i++;
    }

    // Serialize to SD card
    MyLog.print("Saving configuration...");
    if (fsMutex.lock(__PRETTY_FUNCTION__)) {
        FsFile file = fs.open(filename, O_WRONLY | O_CREAT | O_TRUNC);
        if (!file) {
            MyLog.print("Failed to open config file for writing: ");
            MyLog.println(filename);
            fsMutex.unlock();
            return;
        }
        serializeJsonPretty(configJson, file);
        file.close();
        fsMutex.unlock();
    }
    MyLog.println("done");
}

void CConfig::loadFromSdCard(SdFs& fs, MyMutex& fsMutex, const String& filename, SensorMap& sensorMap, OneWireManager* oneWireManager, CNeohubManager& neohub)
{
    MyLog.print("Loading configuration...");

    String contents;
    if (fsMutex.lock(__PRETTY_FUNCTION__)) {
        FsFile file = fs.open(filename, O_RDONLY);
        if (!file) {
            fsMutex.unlock();
            MyLog.println("Failed to open config file");
            this->applyDefaults(sensorMap, oneWireManager);
            return;
        }

        contents = file.readString();
        file.close();
        fsMutex.unlock();
    }

    JsonDocument configJson;
    DeserializationError error = deserializeJson(configJson, contents);
    if (error) {
        MyLog.print("Failed to parse config file: ");
        MyLog.println(error.c_str());
        this->applyDefaults(sensorMap, oneWireManager);
        return;
    }

    hostname = configJson["hostname"].as<String>();
    neohubAddress = configJson["neohubAddress"].as<String>();
    neohubToken = configJson["neohubToken"].as<String>();
    heatingControllerAddress = configJson["heatingControllerAddress"].as<String>();

    flowMaxSetpoint = configJson["flowMaxSetpoint"];
    flowMinSetpoint = configJson["flowMinSetpoint"];

    flowSensorId   = configJson["flowSensorId"].as<String>();
    inputSensorId  = configJson["inputSensorId"].as<String>();
    returnSensorId = configJson["returnSensorId"].as<String>();

    flowProportionalGain   = configJson["flowProportionalGain"].as<double>();
    flowIntegralSeconds    = configJson["flowIntegralSeconds"].as<double>(); 
    flowValveInverted      = configJson["flowValveInverted"].as<bool>();

    roomSetpoint         = configJson["roomSetpoint"].as<double>();
    roomProportionalGain = configJson["roomProportionalGain"].as<double>();
    roomIntegralMinutes  = configJson["roomIntegralMinutes"].as<double>();

    // Iterate over sensors
    JsonArray sensorsArray = configJson["sensors"].as<JsonArray>();
    sensorMap.clear();
    for (JsonObject sensorObj : sensorsArray) {
        sensorMap.setNameForId(sensorObj["id"].as<String>(), sensorObj["name"].as<String>());
    }

    // Iterate over zones
    JsonArray activeZones = configJson["activeZones"].as<JsonArray>();
    neohub.clearActiveZones();
    for (JsonObject zone : activeZones) {
        neohub.addActiveZone(NeohubZone(zone["id"].as<int>(), zone["name"].as<String>()));
    }

    JsonArray monitoredZones = configJson["monitoredZones"].as<JsonArray>();
    neohub.clearMonitoredZones();
    for (JsonObject zone : monitoredZones) {
        neohub.addMonitoredZone(NeohubZone(zone["id"].as<int>(), zone["name"].as<String>()));
    }

    MyLog.println("done");
}

void CConfig::print(CMyLog& p) const
{
    p.println("Config:");
    p.printf("  hostname: %s\n", hostname.c_str());
    p.printf("  Room: %.1f, Kp = %.1f, Ti = %.0f minutes\n", roomSetpoint, roomProportionalGain, roomIntegralMinutes);
    p.printf("  Flow: %.1f-%.1f, Kp = %.1f, Ti = %.0f seconds\n", flowMinSetpoint, flowMaxSetpoint, flowProportionalGain, flowIntegralSeconds);
}

void CConfig::applyDefaults(SensorMap& sensorMap, OneWireManager* oneWireManager)
{
    this->roomSetpoint = 20.0;
    this->roomProportionalGain = 5.0;
    this->roomIntegralMinutes = 180;

    this->flowMinSetpoint     = 25.0;
    this->flowMaxSetpoint     = 37.0;
    
    this->flowProportionalGain    = 3;
    this->flowIntegralSeconds     = 10;
}
