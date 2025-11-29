#ifndef __MY_LOG_H
#define __MY_LOG_H

#include <SdFat.h>    // SdFat by Bill Greiman, version 2.3.0
#include "MyRtc.h"
#include "MyMutex.h"

class CMyLog : public Print {
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

    virtual size_t write(uint8_t c) override;
    virtual size_t write(const uint8_t *buffer, size_t size);
    using Print::write;

    void printlnSdOnly(const char[]);

};

extern CMyLog MyLog;
extern CMyLog MyWebLog;
extern CMyLog MyCrashLog;

// Thread-safe wrapper for HardwareSerial (or any Print)
class CMyDebugLog : public Print {
public:
  CMyDebugLog() : _mutex(xSemaphoreCreateMutex()) {}

  // --- Print interface ---
  virtual size_t write(uint8_t c) override;
  virtual size_t write(const uint8_t *buffer, size_t size) override;
  using Print::write;   // Pull in other Print::write overloads (String, const char*, etc.)

  // Special function for printing with task name and invoking function
  virtual size_t printfWithLocation(const char *debugLogFormat, const char *taskName, const char *location, const char *format, ...);

private:
  SemaphoreHandle_t _mutex;
};

// Global instance, like Serial
extern CMyDebugLog MyDebugLog;
#define DEBUG_LOG_FORMAT "%-20.20s | %-60.60s | "
#define DEBUG_LOG(fmt, ...) MyDebugLog.printfWithLocation(DEBUG_LOG_FORMAT, pcTaskGetName(nullptr), __PRETTY_FUNCTION__, (fmt), ##__VA_ARGS__)

#endif