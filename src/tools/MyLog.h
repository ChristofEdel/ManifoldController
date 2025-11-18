#ifndef __MY_LOG_H
#define __MY_LOG_H

#include <SdFat.h>    // SdFat by Bill Greiman, version 2.3.0
#include "MyRtc.h"
#include "MyMutex.h"

class CMyLog {
  private:
    bool m_logToSerial = false;
    bool m_logToSdCard = false;
    const char *m_logFileName = nullptr;
    CMyRtc *m_rtc = nullptr;
    SdFs *m_sd = nullptr;
    MyMutex *m_sdMutex = nullptr;
    MyMutex *m_loggerMutex = nullptr;
    bool m_atStartOfLine = true;

    FsFile openLogFile(void);
    void handleStartOfLine(void);
    void printRaw(const char *s);

    bool lockSdCard();
    void unlockSdCard();

  public:
    CMyLog() : m_loggerMutex(new MyMutex()) {};
    void enableSdCardLog(const char *logFileName, SdFs *sdFs, MyMutex *sdMutex);
    void enableSerialLog();
    void logTimestamps(CMyRtc &pRtc) { this->m_rtc = &pRtc; };

    void print(const __FlashStringHelper *);
    void print(const String &);
    void print(const char[]);
    void print(char);
    void print(unsigned char, int = DEC);
    void print(int, int = DEC);
    void print(unsigned int, int = DEC);
    void print(long, int = DEC);
    void print(unsigned long, int = DEC);
    void print(double, int = 2);
    void print(const Printable&);

    void println(const __FlashStringHelper *);
    void println(const String &s);
    void println(const char[]);
    void println(char);
    void println(unsigned char, int = DEC);
    void println(int, int = DEC);
    void println(unsigned int, int = DEC);
    void println(long, int = DEC);
    void println(unsigned long, int = DEC);
    void println(double, int = 2);
    void println(const Printable&);
    void println(void);

    void printlnSdOnly(const char[]);

};

extern CMyLog MyLog;
extern CMyLog MyWebLog;
extern CMyLog MyCrashLog;

#endif