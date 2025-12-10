#ifndef __ESPTOOLS_H__
#define __ESPTOOLS_H__

#include <Arduino.h>
bool mustClearRtcMemory();

String getResetReason();
uint32_t getSoftwareResetReason();
const char *getSoftwareResetMessage();
void clearSoftwareResetReason();

void softwareReset(uint32_t reason);
void softwareReset(uint32_t reason, const char *format, ...);
void softwareAbort(uint32_t reason);
void softwareAbort(uint32_t reason, const char *format, ...);

void setupOta();

#define SW_RESET_OUT_OF_MEMORY 0xabcabcab
#define SW_RESET_OTA_UPDATE 0xdefdefde
#define SW_RESET_PANIC_TEST 0x12344321
#define SW_RESET_WEBSOCKET_ABORT 0x34561729
#define SW_RESET_MUTEX_TIMEOUT 0x61254375
#endif // __ESPTOOLS_H__
