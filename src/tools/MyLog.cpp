#include "MyLog.h"
#include "MyRtc.h"
#include <SdFat.h>
#include "BackgroundFileWriter.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CMyLog - a logger which can log to serial, SD card, or both, with timestamps
//

void CMyLog::enableSdCardLog(const char *fileName) {
  this->m_logFileName = fileName;
  this->m_logToSdCard = true;
}

void CMyLog::enableSerialLog() {
  this->m_logToSerial = true;
}

// Single-byte write - this does all the work because we want to have
// a timestamp at the beginning of every line
size_t CMyLog::write(uint8_t c) {

    // If we are at the start of a line, write a timestamp
    // this may recursively call write()
    handleStartOfLine();

    // If the buffer is full, we write it first
    if (this->m_bufferLen >= sizeof(this->m_buffer)) {
        this->flush();
    }

    // Add character to buffer
    this->m_buffer[this->m_bufferLen++] = c;

    // if the character was a newline, flush the buffer
    if (c == '\n') {
        this->flush();
        this->m_atStartOfLine = true;
    }
    return 1;
}

// Write a series of characters in a buffer
size_t CMyLog::write(const uint8_t* buffer, size_t size)
{
    while (size--) write (*buffer++);
    return size;
}

void CMyLog::flush()
{
    if (this->m_bufferLen == 0) return;

    if (this->m_logToSerial) MyDebugLog.write(this->m_buffer, this->m_bufferLen);
    if (this->m_logToSdCard) BackgroundFileWriter.write(this->m_logFileName, this->m_buffer, this->m_bufferLen);

    this->m_bufferLen = 0;
}

// Special line write to SD card only - used to avoid cluttering the serial log
void CMyLog::printlnSdOnly(const char s[])
{
    bool serial = this->m_logToSerial;
    this->m_logToSerial = false;
    this->println(s);
    this->m_logToSerial = serial;
}

void CMyLog::handleStartOfLine()
{
    if (this->m_atStartOfLine) {
        this->m_atStartOfLine = false;
        if (this->m_rtc) {
            MyRtcTime t = this->m_rtc->getTime();
            if (t.isValid()) {
                this->write(this->m_rtc->getTime().getTimestampText().c_str());
                this->write(" | ");
            }
            else {
                this->write("????-??-?? ??:??:?? | ");
            }
        }
        else {
            this->write("????-??-?? ??:??:?? | ");
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
//
// CMyDebugLog - a thread-safe wrapper for stdout
//

// Single-byte write
size_t CMyDebugLog::write(uint8_t c)
{
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        size_t n = fwrite(&c, 1, 1, stdout);
        fflush(stdout);
        xSemaphoreGive(_mutex);
        return n;
    }
    return 0;
}

// Buffer write
size_t CMyDebugLog::write(const uint8_t* buffer, size_t size)
{
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        size_t n = fwrite(buffer, size, 1, stdout);
        fflush(stdout);
        xSemaphoreGive(_mutex);
        return n;
    }
    return 0;
}

// Print with task name and location in code - used by DEBUG_LOG macro
size_t CMyDebugLog::printfWithLocation(const char* debugLogFormat, const char* taskName, const char* location, const char* format, ...)
{
    if (xSemaphoreTake(_mutex, portMAX_DELAY) == pdTRUE) {
        int result = fprintf(stdout, debugLogFormat, taskName, location);

        // The following is 1:1 copy of the printf implementation but with a larger buffer
        // and writing to stdout
        char loc_buf[200];
        char* temp = loc_buf;
        va_list arg;
        va_list copy;
        va_start(arg, format);
        va_copy(copy, arg);
        int len = vsnprintf(temp, sizeof(loc_buf), format, copy);
        va_end(copy);
        if (len < 0) {
            va_end(arg);
            xSemaphoreGive(_mutex);
            return result;
        }
        if (len >= (int)sizeof(loc_buf)) {  // comparation of same sign type for the compiler
            temp = (char*)malloc(len + 1);
            if (temp == NULL) {
                va_end(arg);
                xSemaphoreGive(_mutex);
                return result;
            }
            len = vsnprintf(temp, len + 1, format, arg);
        }
        va_end(arg);
        len = fwrite(temp, len, 1, stdout);
        if (temp != loc_buf) {
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

