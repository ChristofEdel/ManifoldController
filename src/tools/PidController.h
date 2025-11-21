#ifndef __PID_CONTROLLER_H
#define __PID_CONTROLLER_H

#include "MyConfig.h"

struct PidControllerConfig {
  double proportionalGain;      // Control constants used directly in control algorithm
  double integralGain;
  double derivativeGain;

  double integralTimeSeconds;   // Alternative way to express integral gain
  double derivativeTimeSeconds; // Alternative way to express derivative gain

  double minOutput;
  double maxOutput;

  PidControllerConfig(double minOutput, double maxOutput) {
    this->minOutput = minOutput;
    this->maxOutput = maxOutput;
  };
};

struct PidControllerState {
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

class PidController {
  private:
    PidControllerConfig m_config;
    PidControllerState m_state;
    double m_input;
    double m_setpoint;
    double m_output;

  public:
    PidController(double minOutput, double maxOutput) : m_config(minOutput, maxOutput) { };
    
    void configureGains(double proportionalGain, double integralGain, double derivativeGain);
    void configureSeconds(double proportionalGain, double integralTimeSeconds, double derivativeTimeSeconds);
    void configureSeconds(double proportionalGain, double integralGain, double integralTimeSeconds, double derivativeGain, double derivativeTimeSeconds);

    void resetState();                                  // Start for the beginning with no accumulater error (yet),
                                                        // output remains unchanged from the current position, but will not
                                                        // be updated until the second-to-next cycle
    void resetState(double initialOutput);              // Start for the beginning with no accumulater error (yet)
                                                        // output starts at the value given

    void setInput(double input);                        // Get/set the current input variable
    double getInput() { return this->m_input; };

    void setSetpoint(double setpoint);                  // Set a new setpoint
    double getSetpoint() { return this->m_setpoint; };

    void setOutput(double position);
    void calculateOutput();
    double getOutput() { return this->m_output; };
};

#endif