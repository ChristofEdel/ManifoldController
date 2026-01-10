#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <Arduino.h>
#include <ArduinoJson.h>

#include "MyLog.h"

class SdFs;

class CConfig {
  private:
    String name;
    String hostname;

    String neohubAddress;
    String neohubToken;
    bool neohubProxyEnabled;
    String heatingControllerAddress;

    String weatherlinkAddress;

    double flowMaxSetpoint;
    double flowMinSetpoint;

    String flowSensorId;
    String inputSensorId;
    String returnSensorId;

    double flowProportionalGain;
    double flowIntegralSeconds;
    bool flowValveInverted;

    double roomSetpoint;
    double roomProportionalGain;
    double roomIntegralMinutes;

    const char* masterFileName = "/flash/config.json";
    const char* secondaryFileName = "/sdcard/config.json";

  public:
    // Getters
    inline const String& getName() const { return name; };
    inline const String& getHostname() const { return hostname; };

    inline const String& getNeohubAddress() const { return neohubAddress; };
    inline const String& getNeohubToken() const { return neohubToken; };
    inline const bool getNeohubProxyEnabled() const { return neohubProxyEnabled; };
    inline const String& getHeatingControllerAddress() const { return heatingControllerAddress; };

    inline const String& getWeatherlinkAddress() const { return weatherlinkAddress; };

    inline double getFlowMaxSetpoint() const { return flowMaxSetpoint; };
    inline double getFlowMinSetpoint() const { return flowMinSetpoint; };

    inline const String& getFlowSensorId() const { return flowSensorId; };
    inline const String& getInputSensorId() const { return inputSensorId; };
    inline const String& getReturnSensorId() const { return returnSensorId; };

    inline double getFlowProportionalGain() const { return flowProportionalGain; };
    inline double getFlowIntegralSeconds() const { return flowIntegralSeconds; };
    inline bool getFlowValveInverted() const { return flowValveInverted; };

    inline double getRoomSetpoint() const { return roomSetpoint; };
    inline double getRoomProportionalGain() const { return roomProportionalGain; };
    inline double getRoomIntegralMinutes() const { return roomIntegralMinutes; };

    // Setters
    inline void setHostname(const String& value) { hostname = value; };
    inline void setName(const String& value) { name = value; };

    inline void setNeohubAddress(const String& value) { neohubAddress = value; };
    inline void setNeohubToken(const String& value) { neohubToken = value; };
    inline void setNeohubProxyEnabled(bool value)  { neohubProxyEnabled = value; };
    inline void setHeatingControllerAddress(const String& value) { heatingControllerAddress = value; };

    inline void setWeatherlinkAddress(const String& value) { weatherlinkAddress = value; };

    inline void setFlowMaxSetpoint(double value) { flowMaxSetpoint = value; };
    inline void setFlowMinSetpoint(double value) { flowMinSetpoint = value; };

    inline void setFlowSensorId(const String& value) { flowSensorId = value; };
    inline void setInputSensorId(const String& value) { inputSensorId = value; };
    inline void setReturnSensorId(const String& value) { returnSensorId = value; };

    inline void setFlowProportionalGain(double value) { flowProportionalGain = value; };
    inline void setFlowIntegralSeconds(double value) { flowIntegralSeconds = value; };
    inline void setFlowValveInverted(bool value) { flowValveInverted = value; };

    inline void setRoomSetpoint(double value) { roomSetpoint = value; };
    inline void setRoomProportionalGain(double value) { roomProportionalGain = value; };
    inline void setRoomIntegralMinutes(double value) { roomIntegralMinutes = value; };

    // Dummies so shared code compiles
    inline float getBoilerDefaultSetpointForHeating() { return 0; };
    inline float getBoilerFlowAddOn() { return 0; };

    void save() const;
    void load();

    void applyDefaults();
    void print(CMyLog& p) const;

private:
    void save(JsonDocument &configJson, const char *fileName) const;

};

extern CConfig Config;
#endif
