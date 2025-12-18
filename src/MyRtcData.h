#ifndef __MY_RTC_DATA_H
#define __MY_RTC_DATA_H


// 
// The data in this structure is preserved across (soft) reboots.
//
// When adding data to this structure, the layout before the additions should
// be preserved.
//

class MyRtcData {

    double m_lastKnownFlowControllerIntegral;
    bool   m_lastKnownFlowControllerIntegralSet;

    double m_lastKnownValveControllerIntegral;
    bool   m_lastKnownValveControllerIntegralSet;

  public:
    double getLastKnownFlowControllerIntegral() const { return m_lastKnownFlowControllerIntegralSet ? m_lastKnownFlowControllerIntegral : -1; }
    bool   getLastKnownFlowControllerIntegralSet() const { return m_lastKnownFlowControllerIntegralSet;}
    void   setLastKnownFlowControllerIntegral(double value) { m_lastKnownFlowControllerIntegral = value; m_lastKnownFlowControllerIntegralSet = true; }

    double getLastKnownValveControllerIntegral() const { return m_lastKnownValveControllerIntegralSet ? m_lastKnownValveControllerIntegral : -1; }
    bool   getLastKnownValveControllerIntegralSet() const { return m_lastKnownValveControllerIntegralSet;}
    void   setLastKnownValveControllerIntegral(double value) { m_lastKnownValveControllerIntegral = value; m_lastKnownValveControllerIntegralSet = true; }
    
    void clear() {
        m_lastKnownFlowControllerIntegral     = 0;
        m_lastKnownFlowControllerIntegralSet  = false;
        m_lastKnownValveControllerIntegral    = 0;
        m_lastKnownValveControllerIntegralSet = false;
    }
};

//
// EspTools uses a canary to determine if the data in the struct above is valid
// after a reboot. If the structure changes fundamentally, the canary value should
// be changed to ensure that it is cleared,
//
#define rtcCanaryValue ((uint32_t) 0x29349D7F)

#endif