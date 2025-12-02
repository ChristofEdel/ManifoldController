#include "MyLog.h"
#include "MyRtc.h"
#include <SdFat.h>

bool CMyLog::lockSdCard() {
  if(!this->m_sdMutex) return true;
  return this->m_sdMutex->lock(__PRETTY_FUNCTION__);
}
void CMyLog::unlockSdCard() {
  if(this->m_sdMutex) this->m_sdMutex->unlock();
}

// Single-byte write
size_t CMyLog::write(uint8_t c) {
  if (this->m_loggerMutex->lock(__PRETTY_FUNCTION__)) {
    this->handleStartOfLine();
    if (this->m_logToSerial) MyDebugLog.write(c);
    if (this->m_logToSdCard && this->lockSdCard()) { FsFile logFile = this->openLogFile(); if (logFile) { logFile.write(c); logFile.close(); } this->unlockSdCard(); }
    this->m_loggerMutex->unlock();
    if (c == '\n') m_atStartOfLine = true;
    return 1;
  } else {
    return 0;
  }
}

// Buffer write
size_t CMyLog::write(const uint8_t *buffer, size_t size) {
  if (this->m_loggerMutex->lock(__PRETTY_FUNCTION__)) {
    this->handleStartOfLine();
    if (this->m_logToSerial) MyDebugLog.write(buffer, size);
    if (this->m_logToSdCard && this->lockSdCard()) { FsFile logFile = this->openLogFile(); if (logFile) { logFile.write(buffer, size); logFile.close(); } this->unlockSdCard(); }
    this->m_loggerMutex->unlock();
    if (buffer && size >= 1 && buffer[size-1] == '\n') m_atStartOfLine = true;
    return size;
  } else {
    return 0;
  }
}

void CMyLog::printlnSdOnly(const char s[]) {
  bool serial = this->m_logToSerial;
  this->m_logToSerial = false;
  this->println(s);
  this->m_logToSerial = serial;
}

void CMyLog::enableSdCardLog(const char *fileName, SdFs *sdFs, MyMutex *sdMutex) {
  this->m_logFileName = fileName;
  this->m_logToSdCard = true;
  this->m_sdMutex = sdMutex;
  this->m_sd = sdFs;
}

void CMyLog::enableSerialLog() {
  this->m_logToSerial = true;
}

void CMyLog::printRaw(const char *s) {
  if (this->m_logToSerial) MyDebugLog.print(s);
  if (this->m_logToSdCard && this->lockSdCard()) { FsFile logFile = this->openLogFile(); if (logFile) { logFile.print(s); logFile.close(); } this->unlockSdCard(); }
}

void CMyLog::handleStartOfLine() {
  if (this->m_atStartOfLine) {
    if (this->m_rtc) {
      MyRtcTime t = this->m_rtc->getTime();
      if (t.isValid()) {
        this->printRaw(this->m_rtc->getTime().getTimestampText().c_str());
        this->printRaw(" | ");
      }
      else {
        this->printRaw("????-??-?? ??:??:?? | ");
      }
    }
    else {
      this->printRaw("????-??-?? ??:??:?? | ");
    }
  }
  this->m_atStartOfLine = false;
}

FsFile CMyLog::openLogFile() {
  if (!this->m_logToSdCard || !this->m_logFileName || !this->m_logFileName[0]) return FsFile();

  FsFile result = m_sd->open(this->m_logFileName, FILE_WRITE);
  if (!result && this->m_logToSerial) {
    MyDebugLog.print("Unable to open log file ");
    MyDebugLog.print(this->m_logFileName);
    MyDebugLog.print(". Stopping file logging");
    this->m_logToSdCard = false;
  }
  return result;
}

// Single-byte write
size_t CMyDebugLog::write(uint8_t c) {
  if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
    size_t n = fwrite(&c,1,1,stdout);
    fflush(stdout);
    xSemaphoreGive(_mutex);
    return n;
  }
  return 0;
}

// Buffer write
size_t CMyDebugLog::write(const uint8_t *buffer, size_t size) {
  if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
    size_t n =fwrite(buffer,size,1,stdout);
    fflush(stdout);
    xSemaphoreGive(_mutex);
    return n;
  }
  return 0;
}

size_t CMyDebugLog::printfWithLocation(const char *debugLogFormat, const char *taskName, const char *location, const char *format, ...) {
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


CMyLog MyLog;
CMyLog MyWebLog;
CMyLog MyCrashLog;
CMyDebugLog MyDebugLog;

