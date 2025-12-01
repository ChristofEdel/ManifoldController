#ifndef __ESPTOOLS_H__
#define __ESPTOOLS_H__

#include <Arduino.h>
String getResetReason();
void softwareReset(uint32_t reason);
void softwareAbort(uint32_t reason);
void setSoftwareResetReason(uint32_t reason);
void clearSoftwareResetReason();
uint32_t getSoftwareResetReason();

void setupOta();

#define SW_RESET_OUT_OF_MEMORY 0xabcabcab
#define SW_RESET_OTA_UPDATE 0xdefdefde


#endif // __ESPTOOLS_H__
