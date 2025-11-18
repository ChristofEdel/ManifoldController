#include "ValveManager.h"
#include "Config.h"
#include <DFRobot_GP8403.h>     // DAC for valve control
DFRobot_GP8403 dac(&Wire,0x58); // I2C address 0x58



void ValveManager::setup()
{
    while (dac.begin() != 0)
    {
        Serial.println("init error");
        delay(1000);
    }
    Serial.println("init succeed");
    // Set DAC output range
    dac.setDACOutRange(dac.eOutputRange10V);

    this->loadConfig();

}

void ValveManager::configureGains(double proportionalGain, double integralGain, double derivativeGain) {
    // Store gains and related constants
    this->m_config.proportionalGain = proportionalGain;
    this->m_config.integralGain = integralGain;
    this->m_config.derivativeGain = derivativeGain;
    this->m_config.integralTimeSeconds = (integralGain != 0) ? (proportionalGain / integralGain) : 0;
    this->m_config.derivativeTimeSeconds = (proportionalGain != 0) ? (derivativeGain / proportionalGain) : 0;

    this->resetPidState(this->outputs.targetValvePosition);
}

void ValveManager::configureSeconds(double proportionalGain, double integralTimeSeconds, double derivativeTimeSeconds) {
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

    this->resetPidState(this->outputs.targetValvePosition);
}

void ValveManager::resetPidState(float initialOutput) {
    this->m_pidState.first = true;
    this->m_pidState.cumulativeError = initialOutput / this->m_config.integralGain;
    this->m_pidState.previousTime = 0;
}


void ValveManager::loadConfig() {
    this->configureSeconds(
        Config.getProportionalGain(),    // proportionalGain
        Config.getIntegralSeconds(),      // integralTimeSeconds
        Config.getDerivativeSeconds()     // derivativeTimeSeconds
    );
}

void ValveManager::setInputs(float inputTemperature, float flowTemperature, float returnTemperature) {
    this->inputs.inputTemperature = inputTemperature;
    this->inputs.flowTemperature = flowTemperature; 
    this->inputs.returnTemperature = returnTemperature;
};

void ValveManager::calculateValePosition() {

    ValveManagerPidState *state = &this->m_pidState;

    // Get the time change in seconds
    unsigned long now = millis();
    double timeChange = (double)(now - state->previousTime) / 1000.0;
    if (timeChange <= 0) return;

    // Calculate current and cumulative errors
    double error = this->m_setpoint - this->inputs.flowTemperature;
    state->cumulativeError += error * timeChange;
    double dErr = (error - state->previousError) / timeChange;

    // Prevent "integral windup" by constraining contribution of the cumulative error to the 
    // output range (0 - 100)
    if (this->m_config.integralGain) {
        state->cumulativeError = constrain(state->cumulativeError, 0, 100 / this->m_config.integralGain);
    }

    // Calculate new output
    double newOutput =
        this->m_config.proportionalGain * error
        + this->m_config.integralGain * state->cumulativeError
        + this->m_config.derivativeGain * dErr
    ;
    this->outputs.targetValvePosition = constrain(newOutput, 0, 100);
}

void ValveManager::setValvePosition(float position) {
    this->outputs.targetValvePosition = position;
};

void ValveManager::sendOutputs() {
    dac.setDACOutVoltage(this->outputs.targetValvePosition * 100, 0); // Scale 0..100% to 0..10,000 for 0..10V
    Serial.print("Valve position set to ");
    Serial.println(this->outputs.targetValvePosition);
};