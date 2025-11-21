#include "PidController.h"
#include "Config.h"
#include <DFRobot_GP8403.h>     // DAC for valve control


void PidController::configureGains(double proportionalGain, double integralGain, double derivativeGain) {
    // Store gains and related constants
    this->m_config.proportionalGain = proportionalGain;
    this->m_config.integralGain = integralGain;
    this->m_config.derivativeGain = derivativeGain;
    this->m_config.integralTimeSeconds = (integralGain != 0) ? (proportionalGain / integralGain) : 0;
    this->m_config.derivativeTimeSeconds = (proportionalGain != 0) ? (derivativeGain / proportionalGain) : 0;

    this->resetState(this->m_output);
}

void PidController::configureSeconds(double proportionalGain, double integralTimeSeconds, double derivativeTimeSeconds) {
    // Store gains and related constants
    this->m_config.proportionalGain = proportionalGain;
    this->m_config.integralTimeSeconds = integralTimeSeconds;
    this->m_config.derivativeTimeSeconds = derivativeTimeSeconds;
    if (integralTimeSeconds != 0) {
        this->m_config.integralGain = proportionalGain / integralTimeSeconds;
    } else {
        this->m_config.integralGain = 0;
    }
    this->m_config.derivativeGain = proportionalGain * derivativeTimeSeconds;

    this->resetState(this->m_output);
}

void PidController::configureSeconds(
    double proportionalGain, 
    double integralGain, double integralTimeSeconds, 
    double derivativeGain, double derivativeTimeSeconds
) {
    // Store gains and related constants
    this->m_config.proportionalGain = proportionalGain;
    this->m_config.integralTimeSeconds = integralTimeSeconds;
    this->m_config.derivativeTimeSeconds = derivativeTimeSeconds;
    if (integralTimeSeconds != 0) {
        this->m_config.integralGain = integralGain / integralTimeSeconds;
    } else {
        this->m_config.integralGain = 0;
    }
    this->m_config.derivativeGain = derivativeGain * derivativeTimeSeconds;

    this->resetState(this->m_output);
}

void PidController::resetState(double initialOutput) {
    this->m_state.first = true;
    if (this->m_config.integralGain) {
        this->m_state.cumulativeError = initialOutput / this->m_config.integralGain;
    }
    else {
        this->m_state.cumulativeError = 0;
    }
    this->m_state.previousTime = 0;
}

void PidController::setInput(double input) {
    this->m_input = input; 
};

void PidController::setSetpoint(double setpoint) { 
    this->m_setpoint = setpoint; 
    //  this->resetState();
};

void PidController::calculateOutput() {

    unsigned long now = millis();

    // if we have no measurement, we skip this iteration 
    if (this->m_input < -50) return;

    // if this is the first time round, we only remember the time and do nothing
    if (this->m_state.first) { 
        this->m_state.first = false;
        this->m_state.previousTime = now;
        this->m_state.previousError = 0;
        return; 
    } 

    // Get the time change in seconds
    double timeChange = (double)(now - this->m_state.previousTime) / 1000.0;
    if (timeChange <= 0) return;


    // Calculate current and cumulative errors
    double error = this->m_setpoint - this->m_input;
    this->m_state.cumulativeError += error * timeChange;
    double dErr = (error - this->m_state.previousError) / timeChange;

    // Prevent "integral windup" by constraining contribution of the cumulative error to the 
    // output range
    if (this->m_config.integralGain) {
        this->m_state.cumulativeError = constrain(
            this->m_state.cumulativeError, 
            this->m_config.minOutput / this->m_config.integralGain, 
            this->m_config.maxOutput / this->m_config.integralGain
        );
    }

    // Calculate new output
    double newOutput =
        this->m_config.proportionalGain * error
        + this->m_config.integralGain * this->m_state.cumulativeError
        + this->m_config.derivativeGain * dErr
    ;
    this->m_output = constrain(newOutput, this->m_config.minOutput, this->m_config.maxOutput);

    // Prepare for next iteration
    this->m_state.previousTime = now;
    this->m_state.previousError = error;

}

void PidController::setOutput(double position) {
    this->m_output = position;
    this->resetState(position);
}