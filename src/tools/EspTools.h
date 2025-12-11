#ifndef __ESPTOOLS_H__
#define __ESPTOOLS_H__

#include <Arduino.h>


////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RTC Memory management

// Check if the contents of the RTC memory can be used at all
bool rtcMemoryIsValid();

// Copy any relevant information from the RTC memory (if available) 
// and remember the start time
void handleReboot();

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Reset diagnostic and initiation
//

// Return the number of seconds since the last reboot
time_t uptime();
String uptimeText();

String getResetReasonText();
esp_reset_reason_t getResetReason();
uint32_t getSoftwareResetReason();
const String& getSoftwareResetMessage();

// Initiate a reset and pass on the reason to the next boot
void softwareReset(uint32_t reason);
void softwareReset(uint32_t reason, const char* format, ...);
void softwareAbort(uint32_t reason);
void softwareAbort(uint32_t reason, const char* format, ...);

// Various reset resons
#define SW_RESET_OUT_OF_MEMORY 0xabcabcab
#define SW_RESET_OTA_UPDATE 0xdefdefde
#define SW_RESET_PANIC_TEST 0x12344321
#define SW_RESET_USER_RESET 0x12344323
#define SW_RESET_WEBSOCKET_ABORT 0x34561729
#define SW_RESET_MUTEX_TIMEOUT 0x61254375


////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Over-The-Air Update
//

// Initialise over-the-air update functionbality
void setupOta();


////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Core dumps
//

class Esp32CoreDump {
    private: 
        size_t m_address = 0;
        size_t m_size = 0;
        const esp_partition_t* m_partition = 0;

    public:
        bool exists();
        size_t size();
        String getFormat();
        size_t read(size_t start, void *buffer, size_t length);
        bool writeBacktrace(Print &out);
        bool remove();
        
    private:
        void ensureInitialised();
        bool m_initialised = false;
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Live Backtrace
//

class Esp32Backtrace {

  private:
    intptr_t m_backtraceAddress[16];
    int      m_depth = 0;

  public:
    Esp32Backtrace(int skip = 0);
    void print(Print& out) const;
    String toString() const;
};


#endif // __ESPTOOLS_H__
