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

void ValveManager::loadConfig() {
    this->m_valveController.configureSeconds(
        Config.getProportionalGain(),    // proportionalGain
        Config.getIntegralSeconds(),     // integralTimeSeconds
        Config.getDerivativeSeconds()    // derivativeTimeSeconds
    );
}

void ValveManager::setInputs(double inputTemperature, double flowTemperature, double returnTemperature) {
    this->inputs.inputTemperature = inputTemperature;
    this->inputs.flowTemperature = flowTemperature; 
    this->inputs.returnTemperature = returnTemperature;
    this->m_valveController.setInput(flowTemperature);
};

void ValveManager::calculateValvePosition() {
    this->m_valveController.calculateOutput();
    this->outputs.targetValvePosition = this->m_valveController.getOutput();
}

void ValveManager::setValvePosition(double position) {
    this->m_valveController.setOutput(position);
}

void ValveManager::sendOutputs() {
    double valveInverted = 100 - this->outputs.targetValvePosition;
    dac.setDACOutVoltage(valveInverted * 100, 0); // Scale 0..100% to 0..10,000 mV (0-10V)
}