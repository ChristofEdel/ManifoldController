#include "MyConfig.h"
#include "MyLog.h"

CConfig Config;

void CConfig::saveToSdCard(SdFs &fs,  MyMutex &fsMutex, const String &filename, const SensorMap &sensorMap) const {
    JsonDocument configJson;

    configJson["hostname"]              = hostname;

    configJson["flowTargetTemp"]        = flowTargetTemp;

    configJson["flowSensorId"]          = flowSensorId;
    configJson["inputSensorId"]         = inputSensorId;
    configJson["returnSensorId"]        = returnSensorId;

    configJson["proportionalGain"]      = proportionalGain;
    configJson["integralSeconds"]       = integralSeconds;  
    configJson["derivativeSeconds"]     = derivativeSeconds;

    configJson["valveInverted"]         = valveInverted;

    for (int i = 0; i < sensorMap.getCount(); i++) {
        SensorMapEntry * entry = sensorMap[i];
        configJson["sensors"][i]["id"] = entry->id;
        configJson["sensors"][i]["name"] = entry->name;
    }

    // Serialize to SD card
    MyLog.print("Saving configuration...");
    if (fsMutex.lock()) {
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

void CConfig::loadFromSdCard(SdFs &fs, MyMutex &fsMutex, const String &filename, SensorMap &sensorMap, SensorManager *oneWireManager) {

    MyLog.print("Loading configuration...");

    String contents;
    if (fsMutex.lock()) {
        FsFile file = fs.open(filename, O_RDONLY);
        if (!file) { 
            fsMutex.unlock();
            MyLog.println("Failed to open config file");
            this->applyDefaults(sensorMap, oneWireManager);
            return;
        }

        contents= file.readString();
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
    flowTargetTemp = configJson["flowTargetTemp"];
    flowSensorId   = configJson["flowSensorId"].as<String>();
    inputSensorId  = configJson["inputSensorId"].as<String>();
    returnSensorId = configJson["returnSensorId"].as<String>();
    proportionalGain   = configJson["proportionalGain"].as<double>();
    integralSeconds    = configJson["integralSeconds"].as<double>(); 
    derivativeSeconds  = configJson["derivativeSeconds"].as<double>();
    valveInverted  = configJson["valveInverted"].as<bool>();

    // Iterate over sensors
    JsonArray sensorsArray = configJson["sensors"].as<JsonArray>();
    sensorMap.clear();
    for (JsonObject sensorObj : sensorsArray) {
        sensorMap.setNameForId(sensorObj["id"].as<String>(), sensorObj["name"].as<String>());
    }

    MyLog.println("done");

}

void CConfig::print(CMyLog &p) const {
    p.println("Config:");
    p.print("  hostname: ");
    p.println(hostname);
    p.print("  flowTargetTemp: ");
    p.println(flowTargetTemp);
    p.print("  flowSensorId: ");
    p.println(flowSensorId);
    p.print("  inputSensorId: ");
    p.println(inputSensorId);
    p.print("  returnSensorId: ");
    p.println(returnSensorId);
}

void CConfig::applyDefaults(SensorMap &sensorMap, SensorManager *oneWireManager) {
    flowTargetTemp     = 35.0;
    this->proportionalGain = 2;
    this->integralSeconds = 10;
}
