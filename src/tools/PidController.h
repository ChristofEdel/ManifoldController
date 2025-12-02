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

};

struct PidControllerState {
  bool first;                   // In the first iteration, supporess D calculation

  unsigned long previousTime;   // The time (millis) of the last calculation
                                // used to scale the calculation of 
                                // integral and derivative terms if there is jitter

  double previousInput;         // The input observerd in the last iteration
                                // used to calculate the derivative term

  double error;                 // The current error used calculating tle last output
  double cumulativeError;       // The cumulative error value (used for integral calculation)

  double proportionalTerm;
  double integralTerm;
  double derivativeTerm;
};

class PidController {
  private:
    PidControllerConfig m_config;
    PidControllerState m_state;
    double m_input;
    double m_setpoint;
    double m_output;

  public:
    void setOutputRange(double minOutput, double maxOutput) { m_config.minOutput = minOutput; m_config.maxOutput = maxOutput; this->setOutput(m_output); };
    
    void configureGains(double proportionalGain, double integralGain, double derivativeGain);
    void configureSeconds(double proportionalGain, double integralTimeSeconds, double derivativeTimeSeconds);
    void configureSeconds(double proportionalGain, double integralGain, double integralTimeSeconds, double derivativeGain, double derivativeTimeSeconds);

    void resetState();                                  // Start for the beginning with no accumulater error (yet),
                                                        // output remains unchanged from the current position, but will not
                                                        // be updated until the second-to-next cycle

    void setInput(double input);                        // Get/set the current input variable
    double getInput() { return this->m_input; };

    void setSetpoint(double setpoint);                  // Get/set the setpoint
    double getSetpoint() { return this->m_setpoint; };

    void setOutput(double position);
    void calculateOutput();
    double getOutput() { return this->m_output; };

    double getProportionalTerm() { return this->m_state.proportionalTerm; };
    double getIntegralTerm() { return this->m_state.integralTerm; };
    double getIntegralGain() { return this->m_config.integralGain; };
};

#endif