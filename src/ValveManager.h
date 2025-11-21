#ifndef __BOILER_MANAGER_H
#define __BOILER_MANAGER_H

#include "MyConfig.h"
#include "PidController.h"

struct ValveManagerInputs {
  double inputTemperature;     // Read from temperature sensors
  double flowTemperature;      
  double returnTemperature;
};

struct ValveManagerOutputs {
  double targetValvePosition; // 0..100 %
};

class ValveManager {
  private:
    PidController m_valveController = PidController(0, 100);
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

