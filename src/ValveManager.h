#ifndef __BOILER_MANAGER_H
#define __BOILER_MANAGER_H

#include "MyConfig.h"

struct ValveManagerConfig {
  double proportionalGain;      // Control constants used directly in control algorithm
  double integralGain;
  double derivativeGain;

  double integralTimeSeconds;   // Alternative way to express integral gain
  double derivativeTimeSeconds; // Alternative way to express derivative gain

};

struct ValveManagerInputs {
  float inputTemperature;     // Read from temperature sensors
  float flowTemperature;      
  float returnTemperature;    
  // float valvePosition;        // Read from the valve itself
};

struct ValveManagerPidState {
  bool first;                   // In the first iteration, supporess D calculation

  unsigned long previousTime;   // The time (millis) of the last calculation

  double previousError;         // The error value observed at the last calculation
  double cumulativeError;       // The cumulative error value (used for integral calculation)

  // Variables - double - tuining
  double Kp;
  double Ki;
  double Kd;
  double divisor;

  bool constrain;
  double minOut;
  double maxOut;
};

struct ValveManagerOutputs {
  float targetValvePosition; // 0..100 %
};

class ValveManager {
  private:
    ValveManagerConfig m_config;
    ValveManagerPidState m_pidState;
    float m_setpoint;

  public:
    volatile ValveManagerInputs inputs;
    volatile ValveManagerOutputs outputs;

    void configureGains(double proportionalGain, double integralGain, double derivativeGain);
    void configureSeconds(double proportionalGain, double integralTimeSeconds, double derivativeTimeSeconds);
    void resetPidState(float initialOutput);

    void setup();
    void loadConfig();
    void setInputs(float inputTemperature, float flowTemperature, float returnTemperature);
    void setSetpoint(float setpoint) { this->m_setpoint = setpoint; };
    void calculateValePosition();
    void setValvePosition(float position);
    void sendOutputs();
};

#endif