#ifndef __BOILER_MANAGER_H
#define __BOILER_MANAGER_H

#include "MyConfig.h"
#include "PidController.h"

struct ValveManagerInputs {
  double roomTemperature;
  double flowTemperature;

  double inputTemperature;      
  double returnTemperature;      
};

struct ValveManagerOutputs {
  double targetFlowTemperature;  // degrees C
  double targetValvePosition;    // 0..100 %
};

class ValveManager {
  private:
    PidController m_flowController;
    PidController m_valveController;

    double m_roomSetpoint;      // Set by from configuration/setter
    double m_flowSetpoint;      // Set by the n_flowController, or by setFlowSetpoint 
    bool m_fixedFlowSetpoint = false;   

    bool m_valveInverted = false;
    bool m_dacInitialised = false;

  public:
    volatile ValveManagerInputs inputs;
    volatile ValveManagerOutputs outputs;

    // Load the initial state of the valve manager. includes loadConfig()
    void setup();

    // Load all parameters from the configuration file.
    void loadConfig();

    // set/get the setpoint for the room temperature
    void setRooomSetpoint(double setpoint) { this->m_roomSetpoint = setpoint; m_flowController.setSetpoint(setpoint); };
    double getRoomSetpoint() { return this->m_roomSetpoint; };

    // get the (intermediate) setpoint for the flow temperature
    void setFlowSetpoint(double setpoint) { this->m_flowSetpoint = setpoint; this->m_fixedFlowSetpoint = true; m_valveController.setSetpoint(setpoint); };
    void clearFlowSetpoint() { this->m_fixedFlowSetpoint = false; }
    double getFlowSetpoint() { return this->m_flowSetpoint; };

    // Set the process variables that the controllers are managing
    void setInputs (
      double roomTemperature, double flowTemperature, 
      double inputTemperature, double returnTemperature
    );

    // Calculate the outputs and send them to the control hardware
    void calculateValvePosition();
    void sendOutputs();

    void setValvePosition(double position);

    //
    double getRoomProportionalTerm() { return this->m_flowController.getProportionalTerm(); };
    double getRoomIntegralTerm() { return this->m_flowController.getIntegralTerm(); };
    double getFlowProportionalTerm() { return this->m_valveController.getProportionalTerm(); };
    double getFlowIntegralTerm() { return this->m_valveController.getIntegralTerm(); };

    double getRoomIntegralGain() { return this->m_flowController.getIntegralGain(); };
    double getFlowIntegralGain() { return this->m_valveController.getIntegralGain(); };


};

#endif

