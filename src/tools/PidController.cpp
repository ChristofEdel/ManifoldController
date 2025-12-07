#include "PidController.h"

#include <DFRobot_GP8403.h>  // DAC for valve control

#include "Config.h"
#include "NeohubManager.h"

void PidController::configureGains(double proportionalGain, double integralGain, double derivativeGain)
{
    // Store gains and related constants
    this->m_config.proportionalGain = proportionalGain;
    this->m_config.integralGain = integralGain;
    this->m_config.derivativeGain = derivativeGain;
    this->m_config.integralTimeSeconds = (integralGain != 0) ? (proportionalGain / integralGain) : 0;
    this->m_config.derivativeTimeSeconds = (proportionalGain != 0) ? (derivativeGain / proportionalGain) : 0;

    this->resetState();
}

void PidController::configureSeconds(double proportionalGain, double integralTimeSeconds, double derivativeTimeSeconds)
{
    // Store gains and related constants
    this->m_config.proportionalGain = proportionalGain;
    this->m_config.integralTimeSeconds = integralTimeSeconds;
    this->m_config.derivativeTimeSeconds = derivativeTimeSeconds;
    if (integralTimeSeconds != 0) {
        this->m_config.integralGain = proportionalGain / integralTimeSeconds;
    }
    else {
        this->m_config.integralGain = 0;
    }
    this->m_config.derivativeGain = proportionalGain * derivativeTimeSeconds;

    this->resetState();
}

void PidController::configureSeconds(
    double proportionalGain,
    double integralGain, double integralTimeSeconds,
    double derivativeGain, double derivativeTimeSeconds)
{
    // Store gains and related constants
    this->m_config.proportionalGain = proportionalGain;
    this->m_config.integralTimeSeconds = integralTimeSeconds;
    this->m_config.derivativeTimeSeconds = derivativeTimeSeconds;
    if (integralTimeSeconds != 0) {
        this->m_config.integralGain = proportionalGain / integralTimeSeconds;
    }
    else {
        this->m_config.integralGain = 0;
    }
    this->m_config.derivativeGain = proportionalGain * derivativeTimeSeconds;

    this->resetState();
}

void PidController::resetState()
{
    // Initialise as at the beginning
    this->m_state.first = true;
    this->m_state.previousInput = 0;
    this->m_state.previousTime = 0;

    // If we don't have a sensible existing input, we do nothing else
    if (this->m_input == NeohubZoneData::NO_TEMPERATURE) {
        this->m_state.error = 0;
        this->m_state.cumulativeError = 0;
        this->m_state.proportionalTerm = 0;
        this->m_state.integralTerm = 0;
        this->m_state.derivativeTerm = 0;
        return;
    }

    // If we have a sensible input, we tweak the integral term so we achieve the current output
    this->m_state.error = this->m_setpoint - this->m_input;
    this->m_state.derivativeTerm = 0;
    this->m_state.proportionalTerm = this->m_config.proportionalGain * this->m_state.error;
    if (this->m_config.integralGain) {
        this->m_state.integralTerm = this->m_output - this->m_state.proportionalTerm - this->m_state.derivativeTerm;
        this->m_state.cumulativeError = this->m_state.integralTerm / this->m_config.integralGain;
    }
    else {
        this->m_state.integralTerm = 0;
        this->m_state.cumulativeError = 0;
    }
}

void PidController::setInput(double input)
{
    this->m_input = input;
};

void PidController::setSetpoint(double setpoint)
{
    this->m_setpoint = setpoint;
};

void PidController::calculateOutput()
{
    unsigned long now = millis();

    // if we have no measurement, we skip this iteration
    if (this->m_input < -50) return;

    // if this is the first time round, we only remember the time and input and do nothing
    if (this->m_state.first) {
        this->m_state.first = false;
        this->m_state.previousTime = now;
        this->m_state.previousInput = this->m_input;
        return;
    }

    // Get the time change in seconds
    double timeChange = (double)(now - this->m_state.previousTime) / 1000.0;
    if (timeChange <= 0) return;

    // Calculate current and cumulative errors
    this->m_state.error = this->m_setpoint - this->m_input;
    this->m_state.cumulativeError += this->m_state.error * timeChange;
    double dErr = (this->m_input - this->m_state.previousInput) / timeChange;

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
    this->m_state.proportionalTerm = this->m_config.proportionalGain * this->m_state.error;
    this->m_state.integralTerm = this->m_config.integralGain * this->m_state.cumulativeError;
    this->m_state.derivativeTerm = this->m_config.derivativeGain * dErr;
    double newOutput =
        this->m_state.proportionalTerm
        + this->m_state.integralTerm
        + this->m_state.derivativeTerm
    ;
    this->m_output = constrain(newOutput, this->m_config.minOutput, this->m_config.maxOutput);

    // Prepare for next iteration
    this->m_state.previousTime = now;
    this->m_state.previousInput = this->m_input;
}

void PidController::setOutput(double newOutput)
{
    if (newOutput < this->m_config.minOutput) newOutput = this->m_config.minOutput;
    if (newOutput > this->m_config.maxOutput) newOutput = this->m_config.maxOutput;
    this->m_output = newOutput;
    this->resetState();
}