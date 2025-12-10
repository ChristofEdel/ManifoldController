#include <Arduino.h>
#include <esp_system.h> // For reset reason
#include <ArduinoOTA.h>
#include "EspTools.h"
#include "MyLog.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Manage RTC memory which survices a software reset (e.g., crash or OTA update)
//
// variables that should survive a reboot should be declared
//
// RTC_NOINIT_ATTR <type> <variable>
//
// This prevents initialisation on reboot, but after a power-on the values will be random.
//
// Before accessing these variables, the code MUST check if they can be used or if they 
// have to be cleared instead
//
bool mustClearRtcMemory()
{
    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason) {
        // case ESP_RST_EXT:
        case ESP_RST_SW:
        case ESP_RST_PANIC:
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:
        case ESP_RST_DEEPSLEEP:
            // case ESP_RST_SDIO:
            return false;
        default:
            return true;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Reset reason
//

RTC_NOINIT_ATTR uint32_t lastSoftwareResetReason;
RTC_NOINIT_ATTR char lastSoftwareResetMessage[1000];

// Get a string describing the reason for the last reset, including the more detailed reason stored in
// lastSoftwareResetReason
String getResetReason()
{
    esp_reset_reason_t reason = esp_reset_reason();
    String result;
    switch (reason) {
        case ESP_RST_UNKNOWN:    result = "Unknown (probably software update)"; break;
        case ESP_RST_POWERON:    result = "Power-on"; break;
        case ESP_RST_EXT:        result = "External pin"; break;
        case ESP_RST_SW:         result = "Software reset"; break;
        case ESP_RST_PANIC:      result = "System panic"; break;
        case ESP_RST_INT_WDT:    result = "Interrupt watchdog"; break;
        case ESP_RST_TASK_WDT:   result = "Task watchdog"; break;
        case ESP_RST_WDT:        result = "Other watchdog"; break;
        case ESP_RST_DEEPSLEEP:  result = "Deep sleep"; break;
        case ESP_RST_BROWNOUT:   result = "Brownout"; break;
        case ESP_RST_SDIO:       result = "SDIO"; break;
        default:                 result = "Other (" + String(reason) + ")"; break;
    }
    uint32_t swReason = getSoftwareResetReason();
    if (swReason != 0) {
        if (reason == ESP_RST_PANIC || (reason == ESP_RST_SW && swReason == SW_RESET_OTA_UPDATE)) {
            switch (swReason)
            {
                case SW_RESET_OTA_UPDATE:       result += " (OTA update)"; break;
                case SW_RESET_OUT_OF_MEMORY:    result += " (out of memory)"; break;
                case SW_RESET_PANIC_TEST:       result += " (test using /panic URL)"; break;
                case SW_RESET_WEBSOCKET_ABORT:  result += " (unexpected WebSocket data)"; break;
                case SW_RESET_MUTEX_TIMEOUT:    result += " (mutex timeout)"; break;
                default:                        result += " (unknown code 0x" + String(swReason, HEX) + ")"; break;
            }
        }
    }
    return result;
}

uint32_t getSoftwareResetReason()
{
    if (mustClearRtcMemory()) clearSoftwareResetReason();
    return lastSoftwareResetReason;
}

const char* getSoftwareResetMessage()
{
    return lastSoftwareResetMessage;
}

void clearSoftwareResetReason()
{
    lastSoftwareResetReason = 0;
    lastSoftwareResetMessage[0] = '\0';
}

static void setSoftwareResetReason(uint32_t reason, const char *format, va_list arg)
{
    lastSoftwareResetReason = reason;
    if (format) {
        vsnprintf(lastSoftwareResetMessage, sizeof(lastSoftwareResetMessage), format, arg);
    }
    else {
        lastSoftwareResetMessage[0] = '\0';
    }
}

void softwareReset(uint32_t reason)
{
    lastSoftwareResetReason = reason;
    lastSoftwareResetMessage[0] = '\0';
    esp_restart();
}

void softwareReset(uint32_t reason, const char* format, ...)
{
    va_list arg;
    va_start(arg, format);
    setSoftwareResetReason(reason, format, arg);
    va_end(arg);

    esp_restart();
}

void softwareAbort(uint32_t reason)
{
    lastSoftwareResetReason = reason;
    lastSoftwareResetMessage[0] = '\0';
    abort();
}

void softwareAbort(uint32_t reason, const char* format, ...)
{
    va_list arg;
    va_start(arg, format);
    setSoftwareResetReason(reason, format, arg);
    va_end(arg);

    abort();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Over-The-Air Update
//

void setupOta()
{
    static int otaProgressPercent;
    // Initialize OTA updates over WiFi
    ArduinoOTA.onStart([]() {
        MyLog.println("OTA Update Start");
        otaProgressPercent = 0;
    });

    ArduinoOTA.onEnd([]() {
        MyLog.println("OTA Update Completed");
        clearSoftwareResetReason();
        lastSoftwareResetReason = SW_RESET_OTA_UPDATE;
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        if (total > 0) {
            int newProgressPercent = (progress * 100) / total;
            if ((newProgressPercent / 20) > (otaProgressPercent / 20)) {
                MyLog.print("OTA Update Progress: ");
                MyLog.print(newProgressPercent);
                MyLog.println("%");
                otaProgressPercent = newProgressPercent;
            }
        }
    });

    ArduinoOTA.onError([](ota_error_t err) {
        MyLog.print("OTA Update Error: ");
        MyLog.println((int)err);
        MyCrashLog.print("OTA Update Error: ");
        MyCrashLog.println((int)err);
    });

    ArduinoOTA.setMdnsEnabled(false);
    ArduinoOTA.begin();
}
