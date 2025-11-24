#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <Arduino.h>
#include <ArduinoJson.h>
#include <SdFat.h>
#include "SensorMap.h"
#include "MyLog.h"

class CConfig {
    private:
        String hostname;
        double flowTargetTemp;
        String flowSensorId;
        String inputSensorId;
        String returnSensorId;

        double proportionalGain;
        double integralSeconds;
        double derivativeSeconds;

        bool valveInverted;

    public:
        inline const String &getHostname()              const { return hostname; }; 
        inline double getFlowTargetTemp()               const { return flowTargetTemp; }; 
        inline const String &getFlowSensorId()          const { return flowSensorId; };
        inline const String &getInputSensorId()         const { return inputSensorId; };
        inline const String &getReturnSensorId()        const { return returnSensorId; };
        inline double getProportionalGain()             const { return proportionalGain; };
        inline double getIntegralSeconds()              const { return integralSeconds; };
        inline double getDerivativeSeconds()            const { return derivativeSeconds; };
        inline bool getValveInverted()                  const { return valveInverted; };

        inline void setHostname(const String &value)         { hostname = value;};
        inline void setFlowTargetTemp(double value)          { flowTargetTemp = value; };
        inline void setFlowSensorId(const String &value)     { flowSensorId = value;};
        inline void setInputSensorId(const String &value)    { inputSensorId = value;};
        inline void setReturnSensorId(const String &value)   { returnSensorId = value;};
        inline void setProportionalGain(double value)        { proportionalGain = value; };
        inline void setIntegralSeconds(double value)         { integralSeconds = value; };
        inline void setDerivativeSeconds(double value)       { derivativeSeconds = value; };
        inline void setValveInverted(bool value)             { valveInverted = value; };


        void saveToSdCard(SdFs &fs, MyMutex &fsMutex, const String &filename, const SensorMap &sensorMap) const;
        void loadFromSdCard(SdFs &fs, MyMutex &fsMutex, const String &filename, SensorMap &sensorMap, SensorManager *oneWireManager);

        void applyDefaults(SensorMap &sensorMap, SensorManager *oneWireManager);
        void print(CMyLog &p) const;

};

extern CConfig Config;
#endif
