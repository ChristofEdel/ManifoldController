#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SdFat.h>
#include "SensorMap.h"
#include "MyLog.h"
#include "../heatingControl/NeohubManager.h"

class CConfig {
    private:
        String hostname;

        double flowSetpoint;
        double flowMaxSetpoint;
        double flowMinSetpoint;

        String flowSensorId;
        String inputSensorId;
        String returnSensorId;

        double proportionalGain;
        double integralSeconds;
        double derivativeSeconds;
        bool valveInverted;

        double roomSetpoint;
        double roomProportionalGain;
        double roomIntegralSeconds;

    public:
        inline const String &getHostname()              const { return hostname; }; 

        inline double getFlowSetpoint()                 const { return flowSetpoint; }; 
        inline double getFlowMaxSetpoint()              const { return flowMaxSetpoint; }; 
        inline double getFlowMinSetpoint()              const { return flowMinSetpoint; }; 

        inline const String &getFlowSensorId()          const { return flowSensorId; };
        inline const String &getInputSensorId()         const { return inputSensorId; };
        inline const String &getReturnSensorId()        const { return returnSensorId; };

        inline double getProportionalGain()             const { return proportionalGain; };
        inline double getIntegralSeconds()              const { return integralSeconds; };
        inline double getDerivativeSeconds()            const { return derivativeSeconds; };
        inline bool getValveInverted()                  const { return valveInverted; };

        inline double getRoomSetpoint()                 const { return roomSetpoint; };
        inline double getRoomProportionalGain()         const { return roomProportionalGain; };
        inline double getRoomIntegralSeconds()          const { return roomIntegralSeconds; };

        inline void setHostname(const String &value)         { hostname = value;};

        inline void setFlowSetpoint(double value)            { flowSetpoint = value; };
        inline void setFlowMaxSetpoint(double value)         { flowMaxSetpoint = value; };
        inline void setFlowMinSetpoint(double value)         { flowMinSetpoint = value; };

        inline void setFlowSensorId(const String &value)     { flowSensorId = value;};
        inline void setInputSensorId(const String &value)    { inputSensorId = value;};
        inline void setReturnSensorId(const String &value)   { returnSensorId = value;};

        inline void setProportionalGain(double value)        { proportionalGain = value; };
        inline void setIntegralSeconds(double value)         { integralSeconds = value; };
        inline void setDerivativeSeconds(double value)       { derivativeSeconds = value; };
        inline void setValveInverted(bool value)             { valveInverted = value; };

        inline void setRoomSetpoint(double value)            { roomSetpoint = value; };
        inline void setRoomProportionalGain(double value)    { roomProportionalGain = value; };
        inline void setRoomIntegralSeconds(double value)     { roomIntegralSeconds = value; };


        void saveToSdCard(SdFs &fs, MyMutex &fsMutex, const String &filename, const SensorMap &sensorMap, CNeohubManager &neohub) const;
        void loadFromSdCard(SdFs &fs, MyMutex &fsMutex, const String &filename, SensorMap &sensorMap, SensorManager *oneWireManager, CNeohubManager &neohub);

        void applyDefaults(SensorMap &sensorMap, SensorManager *oneWireManager);
        void print(CMyLog &p) const;

};

extern CConfig Config;
#endif
