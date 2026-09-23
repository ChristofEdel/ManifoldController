#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <Arduino.h>
#include <ArduinoJson.h>
#include "ValveManagerControlMode.h"

#include "MyLog.h"

class SdFs;

class CConfig {
  private:
    String name;
    String hostname;

    String mqttHost;
    int mqttPort = 1883;
    String mqttUsername;
    String mqttPassword;

    String mqttTopicNeohub;
    String mqttTopicTemperature;
    String mqttTopicTemperatureKeepalive;

    double flowMaxSetpoint = std::numeric_limits<double>::quiet_NaN();
    double flowMinSetpoint = std::numeric_limits<double>::quiet_NaN();
    double flowAddOn = std::numeric_limits<double>::quiet_NaN();

    String flowSensorId;
    String inputSensorId;
    String returnSensorId;

    double flowProportionalGain = std::numeric_limits<double>::quiet_NaN();
    double flowIntegralSeconds = std::numeric_limits<double>::quiet_NaN();
    bool flowValveInverted;

    double roomSetpoint = std::numeric_limits<double>::quiet_NaN();
    double roomProportionalGain = std::numeric_limits<double>::quiet_NaN();
    double roomIntegralMinutes = std::numeric_limits<double>::quiet_NaN();

    ValveManagerControlMode controlMode;
    double weatherControlOat = std::numeric_limits<double>::quiet_NaN();
    double weatherControlFlow = std::numeric_limits<double>::quiet_NaN();
    double weatherControlExponent = std::numeric_limits<double>::quiet_NaN();
    double hybridTweakBandWidth = std::numeric_limits<double>::quiet_NaN();
    double fallbackFlow = std::numeric_limits<double>::quiet_NaN();

    const char* masterFileName = "/flash/config.json";
    const char* secondaryFileName = "/sdcard/config.json";

  public:
    // Getters
    inline const String& getName() const { return name; };
    inline const String& getHostname() const { return hostname; };

    inline const String& getMqttHost() const { return mqttHost; }
    inline int getMqttPort() const { return mqttPort; }
    inline const String& getMqttUsername() const { return mqttUsername; }
    inline const String& getMqttPassword() const { return mqttPassword; }

    inline const String& getMqttTopicNeohub() const { return mqttTopicNeohub; }
    inline const String& getMqttTopicTemperature() const { return mqttTopicTemperature; }
    inline const String& getMqttTopicTemperatureKeepalive() const { return mqttTopicTemperatureKeepalive; }

    inline double getFlowMaxSetpoint() const { return flowMaxSetpoint; };
    inline double getFlowMinSetpoint() const { return flowMinSetpoint; };
    inline double getFlowAddOn() const { return flowAddOn; };

    inline const String& getFlowSensorId() const { return flowSensorId; };
    inline const String& getInputSensorId() const { return inputSensorId; };
    inline const String& getReturnSensorId() const { return returnSensorId; };

    inline double getFlowProportionalGain() const { return flowProportionalGain; };
    inline double getFlowIntegralSeconds() const { return flowIntegralSeconds; };
    inline bool getFlowValveInverted() const { return flowValveInverted; };

    inline double getRoomSetpoint() const { return roomSetpoint; };
    inline double getRoomProportionalGain() const { return roomProportionalGain; };
    inline double getRoomIntegralMinutes() const { return roomIntegralMinutes; };

    inline ValveManagerControlMode getControlMode() const { return controlMode; };
    inline double getWeatherControlOat() const { return weatherControlOat; };
    inline double getWeatherControlFlow() const { return weatherControlFlow; };
    inline double getWeatherControlExponent() const { return weatherControlExponent; };

    inline double getHybridTweakBandWidth() const { return hybridTweakBandWidth; };

    inline double getFallbackFlow() const { return fallbackFlow; };

    // Setters
    inline void setHostname(const String& value) { hostname = value; };
    inline void setName(const String& value) { name = value; };

    inline void setMqttHost(const String& value) { mqttHost = value; }
    inline void setMqttPort(int value) { mqttPort = value; }
    inline void setMqttUsername(const String& value) { mqttUsername = value; }
    inline void setMqttPassword(const String& value) { mqttPassword = value; }
    inline void setMqttTopicNeohub(const String& value) { mqttTopicNeohub = value; }
    inline void setMqttTopicTemperature(const String& value) { mqttTopicTemperature = value; }
    inline void setMqttTopicTemperatureKeepalive(const String& value) { mqttTopicTemperatureKeepalive = value; }
    
    inline void setFlowMaxSetpoint(double value) { flowMaxSetpoint = value; };
    inline void setFlowMinSetpoint(double value) { flowMinSetpoint = value; };
    inline void setFlowAddOn(double value) { flowAddOn = value; };

    inline void setFlowSensorId(const String& value) { flowSensorId = value; };
    inline void setInputSensorId(const String& value) { inputSensorId = value; };
    inline void setReturnSensorId(const String& value) { returnSensorId = value; };

    inline void setFlowProportionalGain(double value) { flowProportionalGain = value; };
    inline void setFlowIntegralSeconds(double value) { flowIntegralSeconds = value; };
    inline void setFlowValveInverted(bool value) { flowValveInverted = value; };

    inline void setRoomSetpoint(double value) { roomSetpoint = value; };
    inline void setRoomProportionalGain(double value) { roomProportionalGain = value; };
    inline void setRoomIntegralMinutes(double value) { roomIntegralMinutes = value; };

    inline void setControlMode(ValveManagerControlMode value) { controlMode = value; };
    inline void setControlMode(int value) { controlMode = (ValveManagerControlMode) value; };
    inline void setWeatherControlOat(double value) { weatherControlOat = value; };
    inline void setWeatherControlFlow(double value) { weatherControlFlow = value; };
    inline void setWeatherControlExponent(double value) { weatherControlExponent = value; };

    inline void setHybridTweakBandWidth(double value) { hybridTweakBandWidth = value; };

    inline void setfallbackFlow(double value) { fallbackFlow = value; };

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
