#ifndef __BOILER_MANAGER_H
#define __BOILER_MANAGER_H

#include "MyConfig.h"
#include "PidController.h"

// The inputs to the contol loops
struct ValveManagerInputs {
  double roomTemperature;   // Room temperature used to regulate the flow
  double flowTemperature;   // Flow temperature used to regulate the valve
                            // taken from the output of the room control

  double inputTemperature;      
  double returnTemperature;      
};

// The output of the control loops
struct ValveManagerOutputs {
  double targetFlowTemperature;  // degrees C
  double targetValvePosition;    // 0..100 %
};

class ValveManager {
  private:
    PidController m_flowController;
    PidController m_valveController;

    double m_roomSetpoint;            // Set by from configuration/setter

    bool m_valveInverted = false;
    bool m_dacInitialised = false;

    bool m_manualValveControl = false;
    double m_manualValvePosition;

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
    double getFlowSetpoint() { return this->outputs.targetFlowTemperature; };
    void setFlowSetpointRange(double min, double max) { this->m_flowController.setOutputRange(min, max); };

    // Set the process variables that the controllers are managing
    void setInputs (
      double roomTemperature, double flowTemperature, 
      double inputTemperature, double returnTemperature
    );

    // Calculate the outputs and send them to the control hardware
    void calculateValvePosition();
    double getValvePosition();
    void sendCurrentValvePosition();

    // Direct setting of a particular PID controller OUTPUT.
    // "Debumps" the corresponding controller so it will initially hold that output 
    void setFlowSetpoint(double setpoint) { this->m_flowController.setOutput(setpoint); this->m_valveController.setSetpoint(setpoint); };
    void setValvePosition(double position);

    // Manual valve control. The controllers continue as normal, but the actual valve position 
    // can be controlled by hand through the UI
    void setManualValvePosition (double position) { m_manualValveControl = true;  m_manualValvePosition = position; }
    void resumeAutomaticValveControl() { m_manualValveControl = false; this->sendCurrentValvePosition(); };
    bool valveUnderManualControl() { return m_manualValveControl; };

    // Information: get the individual components of the full PID output calculation 
    double getRoomProportionalTerm() { return this->m_flowController.getProportionalTerm(); };
    double getRoomIntegralTerm() { return this->m_flowController.getIntegralTerm(); };
    double getFlowProportionalTerm() { return this->m_valveController.getProportionalTerm(); };
    double getFlowIntegralTerm() { return this->m_valveController.getIntegralTerm(); };

    double getRoomIntegralGain() { return this->m_flowController.getIntegralGain(); };
    double getFlowIntegralGain() { return this->m_valveController.getIntegralGain(); };


  private: 
    void sendValvePosition (double valvePosition);

};

#endif

