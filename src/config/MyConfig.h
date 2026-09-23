#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <Arduino.h>
#include <type_traits>
#include "MyLog.h"
#include "Homeassistant.h"
#include "ValveManagerControlMode.h"

///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Configuration parameters
//
// Loads, maintains and saves configuration parameters using a json file
// Publishes configuration parameters over MQTT and allows changing them via MQTT
//
///////////////////////////////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------------------------------
// #region Single source of truth for all configuration parameters
//-------------------------------------------------------------------------------------------------

#define CONFIG_PARAMETERS(X) \
    X(String,                  Name,                          , "") \
    X(String,                  Hostname,                      , "") \
    \
    X(String,                  MqttHost,                      , "") \
    X(long,                    MqttPort,                      , "") \
    X(String,                  MqttUsername,                  , "") \
    X(String,                  MqttPassword,                  , "") \
    \
    X(String,                  MqttTopicNeohub,               , "") \
    X(String,                  MqttTopicTemperature,          , "") \
    X(String,                  MqttTopicTemperatureKeepalive, , "") \
    \
    X(double,                  FlowMaxSetpoint,               Temperature, "Min Flow") \
    X(double,                  FlowMinSetpoint,               Temperature, "Max Flow") \
    X(double,                  FlowAddOn,                     Temperature, "Flow add-on") \
    \
    X(String,                  FlowSensorId,                  , "") \
    X(String,                  InputSensorId,                 , "") \
    X(String,                  ReturnSensorId,                , "") \
    \
    X(double,                  FlowProportionalGain,          , "") \
    X(double,                  FlowIntegralSeconds,           , "") \
    X(bool,                    FlowValveInverted,             , "") \
    \
    X(double,                  RoomSetpoint,                  Temperature, "Room Setpoint") \
    X(double,                  RoomProportionalGain,          , "") \
    X(double,                  RoomIntegralMinutes,           , "") \
    \
    X(long,                    ControlModeAsInt,              , "") \
    \
    X(double,                  WeatherControlOat,             , "") \
    X(double,                  WeatherControlFlow,            , "") \
    X(double,                  WeatherControlExponent,        , "") \
    \
    X(double,                  HybridTweakBandWidth,          , "") \
    \
    X(double,                  FallbackFlow,                  , "")

    
// #endregion
//-------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------
// #region CConfig class and Config global singleton
//-------------------------------------------------------------------------------------------------

class CConfig {

  private:

    // data members -------------------------------------------------------------------------------

    #define CONFIG_DECLARE(type, name, pType, displayName) \
        type _##name;

    CONFIG_PARAMETERS(CONFIG_DECLARE)

    #undef CONFIG_DECLARE

public:

    // getters/setters and manipulation -----------------------------------------------------------

    #define CONFIG_ACCESSORS(type, name, pType, displayName) \
        const type& get##name() const { return _##name; } \
        void set##name(const type& value) { _##name = value; }

        CONFIG_PARAMETERS(CONFIG_ACCESSORS)

    #undef CONFIG_ACCESSORS

    // Specials: hostname before the first dot, and propertly typed control mode
    String getHostnameFirstPart();
    ValveManagerControlMode getControlMode() { 
        return (ValveManagerControlMode) this->_ControlModeAsInt;
    }
    void setControlMode(ValveManagerControlMode mode) { 
        this->_ControlModeAsInt = (long) mode;
    }

    // Dummies so shared code compiles
    inline float getBoilerDefaultSetpointForHeating() { return 0; };
    inline float getBoilerFlowAddOn() { return 0; };

    void applyDefaults();


    // saving / loading from flash  ---------------------------------------------------------------
  public:
    void save() const;
    void load();
  private:
    const char* masterFileName = "/flash/config.json";
    const char* secondaryFileName = "/sdcard/config.json";
    static String _toParameterKey(const String& name);
    void save(JsonDocument &configJson, const char *fileName) const;

    // Sending to MQTT  ---------------------------------------------------------------------------
  public:
    void publishConfigToMqtt(const String& deviceId, int retentionSeconds);
    bool publishAutodiscoverTopics(const String& deviceId);
  private:
    int _retentionSeconds = 5 * 60;
    void _publishTemperatureAutodiscover(HomeassistantMqttDiscovery &d, const String& name, const String& displayName);
    void _publishAutodiscover(HomeassistantMqttDiscovery &d, const String& name, const String& displayName) { return; } // dummy for parameters which are not published

    // Updating from MQTT -------------------------------------------------------------------------
  public:
    void subscribeToMqttSetTopics(const String& deviceId); 
  private:
    String _deviceId;

    void _updateFromMqtt(const String& topic, const String& payload);
    static void _mqttCallback(void *arg, const String& topic, const String& payload);

};

extern CConfig Config;

#endif
