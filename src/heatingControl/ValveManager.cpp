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
        Config.getFlowProportionalGain(),    // proportionalGain
        Config.getFlowIntegralSeconds(),     // integralTimeSeconds
        0                                    // derivativeTimeSeconds
    );
    this->m_valveInverted = Config.getFlowValveInverted();
}

void ValveManager::setInputs(
    double roomTemperature, double flowTemperature, 
    double inputTemperature, double returnTemperature
) {
    this->inputs.roomTemperature = roomTemperature;
    this->inputs.flowTemperature = flowTemperature; 
    this->inputs.inputTemperature = inputTemperature;
    this->inputs.returnTemperature = returnTemperature;
};

void ValveManager::calculateValvePosition() {

    // If we have a valid room temperature, we recalculate the flow
    // temperature
    if (this->inputs.roomTemperature != NeohubZoneData::NO_TEMPERATURE) {
        this->m_flowController.setInput(this->inputs.roomTemperature);
        this->m_flowController.calculateOutput();
    }
    this->outputs.targetFlowTemperature = this->m_flowController.getOutput();
    // If we don't have a valid room temperature, the target flow
    // temperature remains unchanged. If we never had a room temperature,
    // this is theconfigured minimum flow temperature

    // Now that we know the target flow temperature, we recalculate
    // the valve position
    this->m_valveController.setInput(this->inputs.flowTemperature);
    this->m_valveController.setSetpoint(this->outputs.targetFlowTemperature);
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