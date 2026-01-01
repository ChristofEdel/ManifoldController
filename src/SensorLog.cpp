#include <Arduino.h>

#include "MyLog.h"
#include "MyMutex.h"
#include "SensorMap.h"
#include "ValveManager.h"
#include "SensorLog.h"
#include "NeohubZoneManager.h"
#include "ValveManager.h"
#include "Filesystem.h"

char sensorDataFileName[] = "/sdcard/Sensor Values yyyy-mm-dd.csv";
char* getSensorDataFileName()
{
    sprintf(sensorDataFileName, "/sdcard/Sensor Values %s.csv", MyRtc.getTime().getDateText().c_str());
    return sensorDataFileName;
}

void logSensors()
{
    char* dataFileName = getSensorDataFileName();
    bool fileExists = false;
    if (Filesystem.lock()) {
        fileExists = !Filesystem.exists(dataFileName);
        Filesystem.unlock();
    }

    String logLine = getSensorLogLine();
    String headerLine;
    if (SensorMap.hasChanged() || !fileExists) {
        String headerLine = getSensorHeaderLine();
        SensorMap.clearChanged();
    }

    if (Filesystem.lock()) {

        File dataFile = Filesystem.open(dataFileName, FILE_APPEND);
        
        if (dataFile) {
            if (headerLine != "") dataFile.println(headerLine);
            dataFile.println(logLine);
            dataFile.close();
            Filesystem.unlock();
        }
        else {
            Filesystem.unlock();
            MyLog.print("error opening ");
            MyLog.println(dataFileName);
        }
    }
}

void logSensorHeaderLine()
{
    String headerLine = getSensorHeaderLine();
    char* dataFileName = getSensorDataFileName();

    if (Filesystem.lock()) {
        File dataFile = Filesystem.open(dataFileName, FILE_APPEND);
        if (dataFile) {
            dataFile.println(headerLine);
            dataFile.close();
        }
        Filesystem.unlock();
    }
}

String getSensorLogLine()
{
    String result;
    result = MyRtc.getTime().getTimestampText();

    // Room control
    //  - Setpoint
    //  - Actual
    result += ",";
    result += String(ValveManager.getRoomSetpoint(), 1);
    result += ",";
    if (ValveManager.inputs.roomTemperature > -50) result += String(ValveManager.inputs.flowTemperature, 1);

    // Manifold control
    //   - Setpoint
    //   - input temperature
    //   - return temperature
    //   - valve position
    //   - flow temperature
    result += ",";
    result += String(ValveManager.getFlowSetpoint(), 1);
    result += ",";
    if (ValveManager.inputs.inputTemperature > -50) result += String(ValveManager.inputs.inputTemperature, 1);
    result += ",";
    if (ValveManager.inputs.returnTemperature > -50) result += String(ValveManager.inputs.returnTemperature, 1);
    result += ",";
    result += String(ValveManager.outputs.targetValvePosition, 1);
    result += "%,";
    if (ValveManager.inputs.flowTemperature > -50) result += String(ValveManager.inputs.flowTemperature, 1);

    // All room sensors

    for (NeohubZone z : NeohubZoneManager.getActiveZones()) {
        NeohubZoneData* d = NeohubZoneManager.getZoneData(z.id);
        if (!d) { result += ",,"; continue; }
        result += ",";
        if (d->roomTemperature != NeohubZoneData::NO_TEMPERATURE) {
            result += String(d->roomTemperature, 1);
        }
        result += ",";
        if (d->floorTemperature != NeohubZoneData::NO_TEMPERATURE) {
            result += String(d->floorTemperature, 1);
        }
    }

    for (NeohubZone z : NeohubZoneManager.getMonitoredZones()) {
        NeohubZoneData* d = NeohubZoneManager.getZoneData(z.id);
        if (!d) { result += ",,"; continue; }
        result += ",";
        if (d->roomTemperature != NeohubZoneData::NO_TEMPERATURE) {
            result += String(d->roomTemperature, 1);
        }
        result += ",";
        if (d->floorTemperature != NeohubZoneData::NO_TEMPERATURE) {
            result += String(d->floorTemperature, 1);
        }
    }

    // All other sensors
    String inputSensorId = Config.getInputSensorId();
    String flowSensorId = Config.getFlowSensorId();
    String returnSensorId = Config.getReturnSensorId();

    int n = SensorMap.getCount();
    for (int i = 0; i < n; i++) {
        SensorMapEntry* entry = SensorMap[i];
        if (entry->id == inputSensorId) continue;
        if (entry->id == flowSensorId) continue;
        if (entry->id == returnSensorId) continue;
        OneWireSensor* sensor = OneWireManager.getSensor(entry->id.c_str());
        float temperature = COneWireManager::SENSOR_NOT_FOUND;
        if (sensor) {
            temperature = sensor->calibratedTemperature();
        }
        result += ",";
        if (temperature > -50) {
            result += String(temperature, 1);
        }
    }
    return result;
}

String getSensorHeaderLine()
{
    String result;
    result = "Time,Room Setpoint,Room,Flow Setpoint,Input,Return,Valve,Flow";

    String inputSensorId = Config.getInputSensorId();
    String flowSensorId = Config.getFlowSensorId();
    String returnSensorId = Config.getReturnSensorId();

    for (NeohubZone z : NeohubZoneManager.getActiveZones()) {
        result += ",";
        result += z.name;
        result += ",";
        result += z.name + "Floor";
    }

    for (NeohubZone z : NeohubZoneManager.getMonitoredZones()) {
        result += ",";
        result += z.name;
        result += ",";
        result += z.name + "Floor";
    }

    int n = SensorMap.getCount();
    for (int i = 0; i < n; i++) {
        SensorMapEntry* entry = SensorMap[i];
        if (entry->id == inputSensorId) continue;
        if (entry->id == flowSensorId) continue;
        if (entry->id == returnSensorId) continue;
        result += ",";
        result += entry->name;
    }

    return result;
}

void logSensorIssues()
{
    int totalReadings = 0;
    int totalCrcErrors = 0;
    int totalNoResponseErrors = 0;
    int totalOtherErrors = 0;
    int totalFailures = 0;
    for (int index = 0; index < OneWireManager.getCount(); index++) {
        OneWireSensor& sensor = OneWireManager[index];
        totalReadings += sensor.readings;
        totalCrcErrors += sensor.crcErrors;
        totalNoResponseErrors += sensor.noResponseErrors;
        totalOtherErrors += sensor.otherErrors;
        totalFailures += sensor.failures;
        sensor.readings = 0;
        sensor.crcErrors = 0;
        sensor.noResponseErrors = 0;
        sensor.otherErrors = 0;
        sensor.failures = 0;
    }
    if (totalReadings > 0 && (totalCrcErrors > 0 || totalNoResponseErrors > 0 || totalFailures > 0)) {
        MyLog.print("Cumulative sensor issues: ");
        if (totalCrcErrors > 0) {
            MyLog.print(totalCrcErrors);
            MyLog.print(" CRC errors (");
            MyLog.print((double)totalCrcErrors * 100.0 / (double)totalReadings, 1);
            MyLog.print("%), ");
        }
        if (totalNoResponseErrors > 0) {
            MyLog.print(totalNoResponseErrors);
            MyLog.print(" missing responses (");
            MyLog.print((double)totalNoResponseErrors * 100.0 / (double)totalReadings, 1);
            MyLog.print("%), ");
        }
        MyLog.print(totalFailures);
        MyLog.print(" failures (");
        MyLog.print((double)totalFailures * 100.0 / (double)totalReadings, 1);
        MyLog.println("%).");
    }
}
