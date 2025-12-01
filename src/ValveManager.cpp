#include "ValveManager.h"
#include "Config.h"
#include <DFRobot_GP8403.h>     // DAC for valve control
DFRobot_GP8403 dac(&Wire,0x5f); // I2C address 0x58



void ValveManager::setup()
{
    int dacInitRetryCount = 3;
    do {
        m_dacInitialised = dac.begin() == 0;
        if (m_dacInitialised) break;
    } while (dacInitRetryCount-- > 0);

    if (!m_dacInitialised) {
        MyLog.println("Unable to initialise DAC");
    }

    // Set DAC output range
    dac.setDACOutRange(dac.eOutputRange10V);

    this->loadConfig();

}

void ValveManager::loadConfig() {

    this->m_flowController.setOutputRange(
        Config.getFlowMinSetpoint(),
        Config.getFlowMaxSetpoint()
    );
    this->m_flowController.configureSeconds(
        Config.getRoomProportionalGain(),       // proportionalGain
        Config.getRoomIntegralMinutes() * 60,   // integralTimeSeconds
        0
    );

    this->m_valveController.setOutputRange(0, 100);
    this->m_valveController.configureSeconds(
        Config.getProportionalGain(),    // proportionalGain
        Config.getIntegralSeconds(),     // integralTimeSeconds
        0                                // derivativeTimeSeconds
    );
    this->m_valveInverted = Config.getValveInverted();
}

void ValveManager::setInputs(
    double roomTemperature, double flowTemperature, 
    double inputTemperature, double returnTemperature
) {
    this->inputs.roomTemperature = roomTemperature;
    this->inputs.flowTemperature = flowTemperature; 
    this->inputs.inputTemperature = inputTemperature;
    this->inputs.returnTemperature = returnTemperature;

    if (this->m_fixedFlowSetpoint) {
        this->m_valveController.setInput(flowTemperature);
    }
    else {
        this->m_flowController.setInput(roomTemperature);
    }
};

void ValveManager::calculateValvePosition() {
    this->m_valveController.calculateOutput();
    this->outputs.targetValvePosition = this->m_valveController.getOutput();
}

void ValveManager::setValvePosition(double position) {
    this->m_valveController.setOutput(position);
}

void ValveManager::sendOutputs() {
    // If the DAC has not been initialised successfully, we try it once again here
    if (!m_dacInitialised) {
        m_dacInitialised = dac.begin() == 0;
        if (m_dacInitialised) MyLog.println("DAC Initialised");
    }

    // No DAC - no action
    if (!m_dacInitialised) return;
    double valvePosition = this->outputs.targetValvePosition;
    if (m_valveInverted) valvePosition = 100 - valvePosition;
    dac.setDACOutVoltage(valvePosition * 100, 0); // Scale 0..100% to 0..10,000 mV (0-10V)
}