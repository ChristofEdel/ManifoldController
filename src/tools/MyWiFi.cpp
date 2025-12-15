#include "MyWiFi.h"

#include <ESPmDNS.h>

#include <NTPClient.h>
#include <SdFat.h>

#include "MyLog.h"

#include "wifi_login.h"

void CMyWiFi::connect()
{
    this->m_wifiStatus = WL_IDLE_STATUS;

    // Set WiFi mode to "Station" (client), then Connect to the network.
    // Wait for up to 10 seconds
    WiFi.mode(WIFI_STA);
    MyLog.print("Establishing connection to Network ");
    MyLog.println(SECRET_WIFI_SSID);
    this->m_wifiStatus = WiFi.begin(SECRET_WIFI_SSID, SECRET_WIFI_PASSWORD);

    int connectTimeoutSeconds = 10;
    const int connectionCheckIntervalMs = 100;
    int connectTimeoutCyclesRemaining = connectTimeoutSeconds * 1000 / connectionCheckIntervalMs;
    do {
        this->m_wifiStatus = WiFi.status();
        if (this->m_wifiStatus == WL_CONNECTED) break;  // Connected - we are done
        delay(connectionCheckIntervalMs);
    } while (connectTimeoutCyclesRemaining--);

    switch (this->m_wifiStatus) {
        case WL_CONNECTED:
            MyLog.println("Connected to WiFi");
            printStatus();
            break;
        case WL_NO_SHIELD:
            MyLog.println("Communication with WiFi module failed!");
            while (true)  // don't continue
                break;
        default:
            MyLog.print("Error: ");
            MyLog.println(this->m_wifiStatus);
    }
}

void CMyWiFi::setHostname(const String& hostname)
{
    if (hostname == "" && this->m_hostname != "") {
        MyLog.println("Hostname cleared");
        MDNS.end();
    }
    else {
        if (!MDNS.begin(hostname)) {
            MyLog.println("Error starting mDNS");
            m_hostname = "";
            return;
        }
        if (hostname != m_hostname) {
            MyLog.print("Hostname set to ");
            MyLog.print(hostname);
            MyLog.println(".local");
        }
    }
    m_hostname = hostname;
}

void CMyWiFi::printStatus()
{
    // print the SSID of the network you're attached to:
    MyLog.print("SSID: ");
    MyLog.println(WiFi.SSID());

    // print your board's IP address:
    IPAddress ip = WiFi.localIP();
    MyLog.print("IP Address: ");
    MyLog.println(ip);

    if (m_hostname != "") {
        MyLog.print("Hostname: ");
        MyLog.println(m_hostname + ".local");
    }

    // print the received signal strength:
    long rssi = WiFi.RSSI();
    MyLog.print("signal strength (RSSI):");
    MyLog.print(rssi);
    MyLog.println(" dBm");
}

String CMyWiFi::getIpAddress() const {
    return WiFi.localIP().toString();
}

static void getSdFatdateTime(uint16_t* fatDate, uint16_t* fatTime)
{
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);

    *fatDate = FAT_DATE(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    *fatTime = FAT_TIME(t.tm_hour, t.tm_min, t.tm_sec);
}

bool CMyWiFi::updateRtcFromTimeServer(CMyRtc* rtc)
{
    NTPClient timeClient(this->m_udp);  // a NTP client to obtain the time
    timeClient.begin();

    if (timeClient.update() && timeClient.isTimeSet()) {
        time_t newUnixTime = timeClient.getEpochTime();
        time_t currentUnixTime = rtc->getTime().getUnixTime();
        timeClient.end();
        FsDateTime::setCallback(getSdFatdateTime);

        long diff = newUnixTime - currentUnixTime;
        if (diff == 0 || abs(diff) == 1) {
            // do not adjust if there is no difference or  less than a second difference
            return true;
        }
        MyRtcTime timeToSet = MyRtcTime(newUnixTime);
        if (abs(diff) > 10000) {
            // If we are way off, we call it "initialising"
            rtc->setTime(timeToSet);
            MyLog.print("Initialising RTC time to ");
            MyLog.println(timeToSet.getTimestampText());
        }
        else {
            MyLog.print("Adjusting RTC time by ");
            MyLog.print(diff);
            MyLog.println(" seconds");
            rtc->setTime(timeToSet);
        }
        return true;
    }
    else {
        timeClient.end();
        return false;
    }
}

CMyWiFi MyWiFi;
