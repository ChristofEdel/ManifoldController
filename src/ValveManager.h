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
  double inputTemperature;     // Read from temperature sensors
  double flowTemperature;      
  double returnTemperature;
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
  double targetValvePosition; // 0..100 %
};

class ValveManager {
  private:
    ValveManagerConfig m_config;
    ValveManagerPidState m_pidState;
    double m_setpoint;

  public:
    volatile ValveManagerInputs inputs;
    volatile ValveManagerOutputs outputs;

    void configureGains(double proportionalGain, double integralGain, double derivativeGain);
    void configureSeconds(double proportionalGain, double integralTimeSeconds, double derivativeTimeSeconds);
    void resetPidState(double initialOutput);

    void setup();
    void loadConfig();
    void setInputs(double inputTemperature, double flowTemperature, double returnTemperature);
    void setSetpoint(double setpoint) { this->m_setpoint = setpoint; };
    double getSetpoint() { return this->m_setpoint; };
    void calculateValvePosition();
    void setValvePosition(double position);
    void sendOutputs();
};

#endif