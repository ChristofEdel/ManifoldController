#ifndef __ESPTOOLS_H__
#define __ESPTOOLS_H__

#include <Arduino.h>

// Check if the RTC memory is valid (after a reboot)
// false after a cold restart where the memory has not been initialised
bool rtcMemoryIsValid();


// Copy any relevant information from the RTC memory (if available) 
// and remember the start time
void handleReboot();

// Return the number of seconds since the last reboot
time_t uptime();
String uptimeText();

// Return information on the last reset that occurred
String getResetReasonText();
uint32_t getSoftwareResetReason();
const String& getSoftwareResetMessage();

// Initiate a reset and pass on the reason to the next boot
void softwareReset(uint32_t reason);
void softwareReset(uint32_t reason, const char* format, ...);
void softwareAbort(uint32_t reason);
void softwareAbort(uint32_t reason, const char* format, ...);

// Initialise over-the-air update functionbality
void setupOta();

// Various reset resons
#define SW_RESET_OUT_OF_MEMORY 0xabcabcab
#define SW_RESET_OTA_UPDATE 0xdefdefde
#define SW_RESET_PANIC_TEST 0x12344321
#define SW_RESET_USER_RESET 0x12344323
#define SW_RESET_WEBSOCKET_ABORT 0x34561729
#define SW_RESET_MUTEX_TIMEOUT 0x61254375
#endif // __ESPTOOLS_H__
