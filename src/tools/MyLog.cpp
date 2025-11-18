#include "MyLog.h"
#include "MyRtc.h"
#include <SdFat.h>

bool CMyLog::lockSdCard() {
  if(!this->m_sdMutex) return true;
  return this->m_sdMutex->lock();
}
void CMyLog::unlockSdCard() {
  if(this->m_sdMutex) this->m_sdMutex->unlock();
}

void CMyLog::print(Printable const&p) {
  this->m_loggerMutex->lock();
  this->handleStartOfLine();
  if (this->m_logToSerial) Serial.print(p);
  if (this->m_logToSdCard && this->lockSdCard()) { FsFile logFile = this->openLogFile(); if (logFile) { logFile.print(p); logFile.close(); } this->unlockSdCard(); }
  this->m_loggerMutex->unlock();
}

void CMyLog::println(Printable const&p) {
  this->m_loggerMutex->lock();
  this->handleStartOfLine();
  if (this->m_logToSerial) Serial.println(p);
  if (this->m_logToSdCard && this->lockSdCard()) { FsFile logFile = this->openLogFile(); if (logFile) { logFile.println(p); logFile.close(); } this->unlockSdCard(); }
  this->m_atStartOfLine = true;
  this->m_loggerMutex->unlock();
}

void CMyLog::print(long i, int base) {
  this->m_loggerMutex->lock();
  this->handleStartOfLine();
  if (this->m_logToSerial) Serial.print(i, base);
  if (this->m_logToSdCard && this->lockSdCard()) { FsFile logFile = this->openLogFile(); if (logFile) { logFile.print(i, base); logFile.close(); } this->unlockSdCard(); }
  this->m_loggerMutex->unlock();
}

void CMyLog::println(long i, int base) {
  this->m_loggerMutex->lock();
  this->handleStartOfLine();
  if (this->m_logToSerial) Serial.println(i, base);
  if (this->m_logToSdCard && this->lockSdCard()) { FsFile logFile = this->openLogFile(); if (logFile) { logFile.println(i, base); logFile.close(); } this->unlockSdCard(); }
  this->m_atStartOfLine = true;
  this->m_loggerMutex->unlock();
}

void CMyLog::print(unsigned long i, int base) {
  this->m_loggerMutex->lock();
  this->handleStartOfLine();
  if (this->m_logToSerial) Serial.print(i, base);
  if (this->m_logToSdCard && this->lockSdCard()) { FsFile logFile = this->openLogFile(); if (logFile) { logFile.print(i, base); logFile.close(); } this->unlockSdCard(); }
  this->m_loggerMutex->unlock();
}

void CMyLog::println(unsigned long i, int base) {
  this->m_loggerMutex->lock();
  this->handleStartOfLine();
  if (this->m_logToSerial) Serial.println(i, base);
  if (this->m_logToSdCard && this->lockSdCard()) { FsFile logFile = this->openLogFile(); if (logFile) { logFile.println(i, base); logFile.close(); } this->unlockSdCard(); }
  this->m_atStartOfLine = true;
  this->m_loggerMutex->unlock();
}

void CMyLog::print(int i, int base) {
  this->m_loggerMutex->lock();
  this->handleStartOfLine();
  if (this->m_logToSerial) Serial.print(i, base);
  if (this->m_logToSdCard && this->lockSdCard()) { FsFile logFile = this->openLogFile(); if (logFile) { logFile.print(i, base); logFile.close(); } this->unlockSdCard(); }
  this->m_loggerMutex->unlock();
}

void CMyLog::println(int i, int base) {
  this->m_loggerMutex->lock();
  this->handleStartOfLine();
  if (this->m_logToSerial) Serial.println(i, base);
  if (this->m_logToSdCard && this->lockSdCard()) { FsFile logFile = this->openLogFile(); if (logFile) { logFile.println(i, base); logFile.close(); } this->unlockSdCard(); }
  this->m_atStartOfLine = true;
  this->m_loggerMutex->unlock();
}

void CMyLog::print(unsigned int i, int base) {
  this->m_loggerMutex->lock();
  this->handleStartOfLine();
  if (this->m_logToSerial) Serial.print(i, base);
  if (this->m_logToSdCard && this->lockSdCard()) { FsFile logFile = this->openLogFile(); if (logFile) { logFile.print(i, base); logFile.close(); } this->unlockSdCard(); }
  this->m_loggerMutex->unlock();
}

void CMyLog::println(unsigned int i, int base) {
  this->m_loggerMutex->lock();
  this->handleStartOfLine();
  if (this->m_logToSerial) Serial.println(i, base);
  if (this->m_logToSdCard && this->lockSdCard()) { FsFile logFile = this->openLogFile(); if (logFile) { logFile.println(i, base); logFile.close(); } this->unlockSdCard(); }
  this->m_atStartOfLine = true;
  this->m_loggerMutex->unlock();
}

void CMyLog::print(double d, int digits) {
  this->m_loggerMutex->lock();
  this->handleStartOfLine();
  if (this->m_logToSerial) Serial.print(d, digits);
  if (this->m_logToSdCard && this->lockSdCard()) { FsFile logFile = this->openLogFile(); if (logFile) { logFile.print(d, digits); logFile.close(); } this->unlockSdCard(); }
  this->m_loggerMutex->unlock();
}

void CMyLog::println(double d, int digits) {
  this->m_loggerMutex->lock();
  this->handleStartOfLine();
  if (this->m_logToSerial) Serial.println(d, digits);
  if (this->m_logToSdCard && this->lockSdCard()) { FsFile logFile = this->openLogFile(); if (logFile) { logFile.println(d, digits); logFile.close(); } this->unlockSdCard(); }
  this->m_atStartOfLine = true;
  this->m_loggerMutex->unlock();
}

void CMyLog::print(String const &s) {
  this->m_loggerMutex->lock();
  this->handleStartOfLine();
  if (this->m_logToSerial) Serial.print(s);
  if (this->m_logToSdCard && this->lockSdCard()) { FsFile logFile = this->openLogFile(); if (logFile) { logFile.print(s); logFile.close(); } this->unlockSdCard(); }
  this->m_loggerMutex->unlock();
}

void CMyLog::println(String const &s) {
  this->m_loggerMutex->lock();
  this->handleStartOfLine();
  if (this->m_logToSerial) Serial.println(s);
  if (this->m_logToSdCard && this->lockSdCard()) { FsFile logFile = this->openLogFile(); if (logFile) { logFile.println(s); logFile.close(); } this->unlockSdCard(); }
  this->m_atStartOfLine = true;
  this->m_loggerMutex->unlock();
}

void CMyLog::print(const char s[]) {
  this->m_loggerMutex->lock();
  this->handleStartOfLine();
  if (this->m_logToSerial) Serial.print(s);
  if (this->m_logToSdCard && this->lockSdCard()) { FsFile logFile = this->openLogFile(); if (logFile) { logFile.print(s); logFile.close(); } this->unlockSdCard(); }
  this->m_loggerMutex->unlock();
}

void CMyLog::println(const char s[]) {
  this->m_loggerMutex->lock();
  this->handleStartOfLine();
  if (this->m_logToSerial) Serial.println(s);
  if (this->m_logToSdCard && this->lockSdCard()) { FsFile logFile = this->openLogFile(); if (logFile) { logFile.println(s); logFile.close(); } this->unlockSdCard(); }
  this->m_atStartOfLine = true;
  this->m_loggerMutex->unlock();
}

void CMyLog::printlnSdOnly(const char s[]) {
  bool serial = this->m_logToSerial;
  this->m_logToSerial = false;
  this->println(s);
  this->m_logToSerial = serial;
}

void CMyLog::println(void) {
  this->m_loggerMutex->lock();
  this->handleStartOfLine();
  if (this->m_logToSerial) Serial.println();
  if (this->m_logToSdCard && this->lockSdCard()) { FsFile logFile = this->openLogFile(); if (logFile) { logFile.println(); logFile.close(); } this->unlockSdCard(); }
  this->m_atStartOfLine = true;
  this->m_loggerMutex->unlock();
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
  if (this->m_logToSerial) Serial.print(s);
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
    Serial.print("Unable to open log file ");
    Serial.print(this->m_logFileName);
    Serial.print(". Stopping file logging");
    this->m_logToSdCard = false;
  }
  return result;
}

CMyLog MyLog;
CMyLog MyWebLog;
CMyLog MyCrashLog;
