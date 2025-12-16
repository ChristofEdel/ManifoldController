#include <SdFat.h>
#include "MyConfig.h"

#include "../heatingControl/NeohubManager.h"
#include "MyLog.h"
#include "SensorMap.h"

CConfig Config;

void CConfig::saveToSdCard(SdFs& fs, MyMutex& fsMutex, const String& filename) const
{
    JsonDocument configJson;

    configJson["name"]                     = name;
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

    for (int i = 0; i < SensorMap.getCount(); i++) {
        SensorMapEntry* entry = SensorMap[i];
        configJson["sensors"][i]["id"] = entry->id;
        configJson["sensors"][i]["name"] = entry->name;
    }

    int i = 0;
    for (auto z : NeohubManager.getActiveZones()) {
        configJson["activeZones"][i]["id"] = z.id;
        configJson["activeZones"][i]["name"] = z.name;
        i++;
    }

    i = 0;
    for (auto z : NeohubManager.getMonitoredZones()) {
        configJson["monitoredZones"][i]["id"] = z.id;
        configJson["monitoredZones"][i]["name"] = z.name;
        i++;
    }

    // Serialize to SD card
    MyLog.print("Saving configuration...");
    if (fsMutex.lock(__PRETTY_FUNCTION__)) {
        FsFile file = fs.open(filename, O_WRONLY | O_CREAT | O_TRUNC);
        if (!file) {
            fsMutex.unlock();
            MyLog.print("Failed to open config file for writing: ");
            MyLog.println(filename);
            return;
        }
        serializeJsonPretty(configJson, file);
        file.close();
        fsMutex.unlock();
    }
    MyLog.println("done");
}

void CConfig::loadFromSdCard(SdFs& fs, MyMutex& fsMutex, const String& filename)
{
    MyLog.print("Loading configuration...");

    String contents;
    if (fsMutex.lock(__PRETTY_FUNCTION__)) {
        FsFile file = fs.open(filename, O_RDONLY);
        if (!file) {
            fsMutex.unlock();
            MyLog.println("Failed to open config file");
            this->applyDefaults();
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
        this->applyDefaults();
        return;
    }

    name                        = configJson["name"] | emptyString;
    hostname                    = configJson["hostname"] | emptyString;
    neohubAddress               = configJson["neohubAddress"] | emptyString;
    neohubToken                 = configJson["neohubToken"] | emptyString;
    heatingControllerAddress    = configJson["heatingControllerAddress"] | emptyString;

    flowMaxSetpoint             = configJson["flowMaxSetpoint"];
    flowMinSetpoint             = configJson["flowMinSetpoint"];

    flowSensorId                = configJson["flowSensorId"] | emptyString;
    inputSensorId               = configJson["inputSensorId"] | emptyString;
    returnSensorId              = configJson["returnSensorId"] | emptyString;

    flowProportionalGain        = configJson["flowProportionalGain"].as<double>();
    flowIntegralSeconds         = configJson["flowIntegralSeconds"].as<double>(); 
    flowValveInverted           = configJson["flowValveInverted"].as<bool>();

    roomSetpoint                = configJson["roomSetpoint"].as<double>();
    roomProportionalGain        = configJson["roomProportionalGain"].as<double>();
    roomIntegralMinutes         = configJson["roomIntegralMinutes"].as<double>();

    // Iterate over sensors
    JsonArray sensorsArray = configJson["sensors"].as<JsonArray>();
    SensorMap.clear();
    for (JsonObject sensorObj : sensorsArray) {
        SensorMap.setNameForId(sensorObj["id"].as<String>(), sensorObj["name"].as<String>());
    }

    // Iterate over zones
    JsonArray activeZones = configJson["activeZones"].as<JsonArray>();
    NeohubManager.clearActiveZones();
    for (JsonObject zone : activeZones) {
        NeohubManager.addActiveZone(NeohubZone(zone["id"].as<int>(), zone["name"].as<String>()));
    }

    JsonArray monitoredZones = configJson["monitoredZones"].as<JsonArray>();
    NeohubManager.clearMonitoredZones();
    for (JsonObject zone : monitoredZones) {
        NeohubManager.addMonitoredZone(NeohubZone(zone["id"].as<int>(), zone["name"].as<String>()));
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

void CConfig::applyDefaults()
{
    this->roomSetpoint = 20.0;
    this->roomProportionalGain = 5.0;
    this->roomIntegralMinutes = 180;

    this->flowMinSetpoint     = 25.0;
    this->flowMaxSetpoint     = 37.0;
    
    this->flowProportionalGain    = 3;
    this->flowIntegralSeconds     = 10;
}
