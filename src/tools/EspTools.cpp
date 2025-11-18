#include <Arduino.h>
#include <esp_system.h> // For reset reason
#include <ArduinoOTA.h>
#include "EspTools.h"
#include "MyLog.h"


RTC_DATA_ATTR uint32_t lastSoftwareResetReason;

// Print a human-readable reset reason
String getResetReason() {
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
  if (lastSoftwareResetReason != 0) {
    if (reason == ESP_RST_PANIC || (reason == ESP_RST_SW && lastSoftwareResetReason == SW_RESET_OTA_UPDATE)) {
      switch (lastSoftwareResetReason)
      {
        case SW_RESET_OTA_UPDATE: result += " (OTA update)"; break;
        case SW_RESET_OUT_OF_MEMORY: result += " (out of memory)"; break;
        default: result += " (unknown code 0x" + String(lastSoftwareResetReason, HEX) + ")"; break;
      }
    }
    lastSoftwareResetReason = 0;
  }
  return result;

}

void setSoftwareResetReason(uint8_t reason) {
  lastSoftwareResetReason = reason;
}

void softwareReset(uint8_t reason) {
  setSoftwareResetReason(reason);
  esp_restart();
}

void softwareAbort(uint8_t reason) {
  setSoftwareResetReason(reason);
  abort();
}

void setupOta()
{
  static int otaProgressPercent;
  // Initialize OTA updates over WiFi
  ArduinoOTA.onStart([]()
                     {
    MyLog.println("OTA Update Start");
    otaProgressPercent = 0; });
  ArduinoOTA.onEnd([]()
                   {
    MyLog.println("OTA Update Completed");
    setSoftwareResetReason(SW_RESET_OTA_UPDATE); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        {
    if (total > 0) {
      int newProgressPercent = (progress * 100) / total;
      if ((newProgressPercent / 20) > (otaProgressPercent / 20)) {
        MyLog.print("OTA Update Progress: ");
        MyLog.print(newProgressPercent);
        MyLog.println("%");
        otaProgressPercent = newProgressPercent;
      }
    } });
  ArduinoOTA.onError([](ota_error_t err)
                     {
    MyLog.print("OTA Update Error: ");
    MyLog.println((int)err);
    MyCrashLog.print("OTA Update Error: ");
    MyCrashLog.println((int)err); });
  ArduinoOTA.begin();
}

