#ifndef __MYWIFI_H
#define __MYWIFI_H

//  WiFi Hardware Support
#include <WiFi.h>
#include "MyRtc.h"

class CMyWiFi {
  
private:
  int m_wifiStatus; // The status of the WiFi connection
  WiFiUDP m_udp;    // A UDP instance to let us send and receive packets over UDP
  String m_hostname;

public:
  void connect();
  void setHostname(const String &hostname);
  const String &getHostname() const { return m_hostname; }
  void printStatus();
  bool updateRtcFromTimeServer(CMyRtc *rtc);
};

extern CMyWiFi MyWiFi;

#endif

