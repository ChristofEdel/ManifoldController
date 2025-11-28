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

// Thread-safe wrapper for HardwareSerial (or any Print)
class CMyDebugLog : public Print {
public:
  CMyDebugLog() : _mutex(xSemaphoreCreateMutex()) {}

  // --- Print interface ---

  // Single-byte write
  virtual size_t write(uint8_t c) override {
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
      size_t n = fwrite(&c,1,1,stdout);
      fflush(stdout);
      xSemaphoreGive(_mutex);
      return n;
    }
    return 0;
  }

  // Buffer write
  virtual size_t write(const uint8_t *buffer, size_t size) override {
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
      size_t n =fwrite(buffer,size,1,stdout);
      fflush(stdout);
      xSemaphoreGive(_mutex);
      return n;
    }
    return 0;
  }

  // Pull in other Print::write overloads (String, const char*, etc.)
  using Print::write;


  virtual size_t printfWithLocation(const char *debugLogFormat, const char *taskName, const char *location, const char *format, ...) {
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {

      int result = fprintf(stdout, debugLogFormat, taskName, location);

      // The following is 1:1 copy of the printf implementation but with a larger buffer
      // and using _serial.write
      char loc_buf[200];
      char * temp = loc_buf;
      va_list arg;
      va_list copy;
      va_start(arg, format);
      va_copy(copy, arg);
      int len = vsnprintf(temp, sizeof(loc_buf), format, copy);
      va_end(copy);
      if(len < 0) {
          va_end(arg);
          xSemaphoreGive(_mutex);
          return result;
      }
      if(len >= (int)sizeof(loc_buf)){  // comparation of same sign type for the compiler
          temp = (char*) malloc(len+1);
          if(temp == NULL) {
              va_end(arg);
              xSemaphoreGive(_mutex);
              return result;
          }
          len = vsnprintf(temp, len+1, format, arg);
      }
      va_end(arg);
      len = fwrite(temp, len, 1, stdout);
      if(temp != loc_buf){
          free(temp);
      }
      // end of copied code

      result += len;
      result += fwrite("\n", 1, 1, stdout);
      fflush(stdout);
      xSemaphoreGive(_mutex);
      return result;
    }
    return 0;
  }

private:
  SemaphoreHandle_t _mutex;
};

// Global instance, like Serial
extern CMyDebugLog MyDebugLog;
#define DEBUG_LOG_FORMAT "%-20.20s | %-60.60s | "
#define DEBUG_LOG(fmt, ...) MyDebugLog.printfWithLocation(DEBUG_LOG_FORMAT, pcTaskGetName(nullptr), __PRETTY_FUNCTION__, (fmt), ##__VA_ARGS__)

#endif