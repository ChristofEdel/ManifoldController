#include "MyRtc.h"

#include <ctime>

#include "MyLog.h"
#include "MyWiFi.h"

bool MyRtcTime::isValid()
{
    return this->m_unixTime > 1609362800L;  // 31 December 2000 - any value below means RTC was not initialised
}

void CMyRtc::start(void) {}

MyRtcTime CMyRtc::getTime()
{
    return MyRtcTime(time(nullptr));
}

bool CMyRtc::setTime(MyRtcTime const& newTime)
{
    struct timeval t = {.tv_sec = newTime.getUnixTime(), .tv_usec = 0};
    settimeofday(&t, nullptr);
    return true;
}

MyRtcTime::MyRtcTime(time_t unixTime)
{
    this->m_unixTime = unixTime;
}

time_t MyRtcTime::getUnixTime() const
{
    return this->m_unixTime;
}

String MyRtcTime::getTimestampText() const
{
    char resultBuffer[50];
    struct tm* currentTime = gmtime(&this->m_unixTime);

    // Format to YYYY-MM-DD HH:MM:SS
    sprintf(
        resultBuffer,
        "%04d-%02d-%02d %02d:%02d:%02d",
        currentTime->tm_year + 1900,
        currentTime->tm_mon + 1,
        currentTime->tm_mday,
        currentTime->tm_hour,
        currentTime->tm_min,
        currentTime->tm_sec
    );
    return String(resultBuffer);
}

String MyRtcTime::getDateText() const
{
    char resultBuffer[50];
    struct tm* currentTime = gmtime(&this->m_unixTime);

    // Format to YYYY-MM-DD
    sprintf(
        resultBuffer,
        "%04d-%02d-%02d",
        currentTime->tm_year + 1900,
        currentTime->tm_mon + 1,
        currentTime->tm_mday
    );
    return String(resultBuffer);
}

CMyRtc MyRtc;
