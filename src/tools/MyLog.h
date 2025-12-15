#ifndef __MY_LOG_H
#define __MY_LOG_H

#include "MyRtc.h"
#include "MyMutex.h"

class SdFs;
class FsFile;


// Logger class which can write to SD card, a serial, or both, with timestamps if required
class CMyLog : public Print {
  private:
    // Where to log to
    bool m_logToSerial = false;
    bool m_logToSdCard = false;
    const char *m_logFileName = nullptr;

    // RTC for timestamps
    CMyRtc *m_rtc = nullptr;

    // Nutex for thread safety of this individual logger
    MyMutex *m_loggerMutex = nullptr;

    // Buffering
    uint8_t m_buffer[200];
    size_t m_bufferLen = 0;

    // Start of line handling
    bool m_atStartOfLine = true;                // Flag indicating we are at start of a line
    void handleStartOfLine(void);               // if we are at start of line, print timestamp

  public:
    CMyLog() : m_loggerMutex(new MyMutex("CMyLog::m_loggerMutex")) {};

    void enableSdCardLog(const char *logFileName);
    void enableSerialLog();
    void logTimestamps(CMyRtc &pRtc) { this->m_rtc = &pRtc; };

    virtual size_t write(uint8_t c) override;
    virtual size_t write(const uint8_t *buffer, size_t size);
    using Print::write;
    void flush();

    void printlnSdOnly(const char[]);

};

// Three global instances of the logger class
extern CMyLog MyLog;
extern CMyLog MyWebLog;
extern CMyLog MyCrashLog;

// Thread-safe wrapper for stdout
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