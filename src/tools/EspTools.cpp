#include "EspTools.h"
#include "StringTools.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <esp_core_dump.h>
#include <esp_err.h>
#include <esp_partition.h>
#include <esp_system.h>  // For reset reason
#include <esp_timer.h>
#include <esp_debug_helpers.h>

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
bool rtcMemoryIsValid()
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
            return true;
        default:
            return false;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// RTC memory contents
//

RTC_NOINIT_ATTR uint32_t rtc_lastSoftwareResetReason;
RTC_NOINIT_ATTR char rtc_lastSoftwareResetMessage[1000];

uint32_t lastSoftwareResetReason = 0;
String lastSoftwareResetMessage;
int64_t lastReboot = 0;
bool rebootHandled = false;


//
// on reboot, copy the RTC memory information for later use and 
// clear the RTC memory
//
void handleReboot() {
    if (rebootHandled) return;
    if (rtcMemoryIsValid()) {
        lastSoftwareResetReason = rtc_lastSoftwareResetReason;
        lastSoftwareResetMessage = rtc_lastSoftwareResetMessage;
    }
    rtc_lastSoftwareResetReason = 0;
    rtc_lastSoftwareResetMessage[0] = '\0';
    lastReboot = esp_timer_get_time();
    rebootHandled = true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Reset diagnostic and initiation
//

time_t uptime() {
    return (esp_timer_get_time() - lastReboot) / 1000000ULL;
}

String uptimeText() 
{
    time_t secs = uptime();
    int d = secs / 86400;  secs %= 86400;
    int h = secs / 3600;   secs %= 3600;
    int m = secs / 60;
    int s = secs % 60;

    String out;

    if (d > 0) { out += d; out += "d "; }
    if (!out.isEmpty() || h > 0) { out += h; out += "h "; }
    if (!out.isEmpty() || m > 0) { out += m; out += "m "; }
    out += s; out += "s";
    return out;
}

// Get a string describing the reason for the last reset, including the more detailed reason stored in
// lastSoftwareResetReason
String getResetReasonText()
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
        switch (swReason)
        {
            case SW_RESET_OTA_UPDATE:       result += " (OTA update)"; break;
            case SW_RESET_OUT_OF_MEMORY:    result += " (out of memory)"; break;
            case SW_RESET_PANIC_TEST:       result += " (test using /panic URL)"; break;
            case SW_RESET_USER_RESET:       result += " (reset by user)"; break;
            case SW_RESET_WEBSOCKET_ABORT:  result += " (unexpected WebSocket data)"; break;
            case SW_RESET_MUTEX_TIMEOUT:    result += " (mutex timeout)"; break;
            default:                        result += " (unknown code 0x" + String(swReason, HEX) + ")"; break;
        }
    }
    return result;
}

esp_reset_reason_t getResetReason()
{
    return esp_reset_reason();
}

uint32_t getSoftwareResetReason()
{
    handleReboot();
    return lastSoftwareResetReason;
}

const String& getSoftwareResetMessage()
{
    handleReboot();
    return lastSoftwareResetMessage;
}

static void setSoftwareResetReason(uint32_t reason, const char *format, va_list arg)
{
    rtc_lastSoftwareResetReason = reason;
    if (format) {
        vsnprintf(rtc_lastSoftwareResetMessage, sizeof(rtc_lastSoftwareResetMessage), format, arg);
    }
    else {
        rtc_lastSoftwareResetMessage[0] = '\0';
    }
}

void softwareReset(uint32_t reason)
{
    rtc_lastSoftwareResetReason = reason;
    rtc_lastSoftwareResetMessage[0] = '\0';
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
    rtc_lastSoftwareResetReason = reason;
    rtc_lastSoftwareResetMessage[0] = '\0';
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
        rtc_lastSoftwareResetReason = SW_RESET_OTA_UPDATE;
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


////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Core dumps
//

void Esp32CoreDump::ensureInitialised() 
{
    if (this->m_initialised) return;
    
    esp_err_t err = esp_core_dump_image_get(&this->m_address, &this->m_size);
    if (err != ESP_OK) {
        this->m_address = 0;
        this->m_size = 0;
        this->m_partition = 0;
    } 
    else {
        this->m_partition = esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA,
            ESP_PARTITION_SUBTYPE_DATA_COREDUMP,
            nullptr
        );
    }
    this->m_initialised = true;
    return;
}

bool Esp32CoreDump::exists()
{
    ensureInitialised();
    return m_size > 0;
}

size_t Esp32CoreDump::size()
{
    ensureInitialised();
    return m_size;
}

String Esp32CoreDump::getFormat()
{
    ensureInitialised();
    if (!this->m_partition) return emptyString;

    size_t addr = 0;
    size_t size = 0;
    if (esp_core_dump_image_get(&addr, &size) != ESP_OK || size < 24) return emptyString;

    uint8_t hdr[4];
    if (esp_partition_read(this->m_partition, addr - this->m_partition->address + 20, hdr, sizeof(hdr)) != ESP_OK)
        return emptyString;

    if (hdr[0] == 0x7F && hdr[1] == 'E' && hdr[2] == 'L' && hdr[3] == 'F')
        return "elf";

    return "binary";
}

size_t Esp32CoreDump::read(size_t start, void *buffer, size_t length)
{
    ensureInitialised();
    if (!this->m_partition) return 0;
    esp_err_t err = esp_partition_read(
        this->m_partition,
        this->m_address - this->m_partition->address + start,
        buffer,
        length
    );
    if (err != ESP_OK) return 0;
    return length;
}

bool Esp32CoreDump::writeBacktrace(Print &out) {
    ensureInitialised();
    if (m_size <= 0) {
        out.println("no core dump available");
        return false;
    }

    esp_core_dump_summary_t summary;
    esp_err_t err = esp_core_dump_get_summary(&summary);
    if (err != ESP_OK) {
        out.println("Core dump summary not available");
        return false;;
    }

    const esp_core_dump_bt_info_t &bt = summary.exc_bt_info;
    if (bt.depth == 0) {
        out.println("No backtrace in core dump");
        return false;
    }
    
    out.print(
        F("py scripts/printSymbols.py")
    );

    size_t depth = bt.depth;
    if (depth > 16) depth = 16;      // summary usually has up to 16 entries

    for (size_t i = 0; i < depth; ++i) {
        out.printf(" 0x%08x", (uint32_t)bt.bt[i]);
    }

    out.print('\n');
    return true;
}

bool Esp32CoreDump::remove() {
    ensureInitialised();
    if (!this->m_partition) return false;
    if (this->m_size == 0) return true;
    
    esp_err_t err = esp_partition_erase_range(this->m_partition, 0, this->m_partition->size);
    return (err == ESP_OK);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Live Backtrace
//

Esp32Backtrace::Esp32Backtrace(int skip /* = 0 */)
{
    esp_backtrace_frame_t frame = { 0 };
    esp_backtrace_get_start(&frame.pc, &frame.sp, &frame.next_pc);
    String m_taskName;

    TaskHandle_t self = xTaskGetCurrentTaskHandle();
    const char* name = pcTaskGetName(self);
    UBaseType_t prio = uxTaskPriorityGet(self);
    this->m_taskName = StringPrintf("%s (%d)", name, prio);

    m_depth = 0;
    for (;;) {
        if (!frame.pc) break;
        if (skip-- <= 0) {
            m_backtraceAddress[m_depth++] = esp_cpu_process_stack_pc(frame.pc);
        }
        if (m_depth >= (sizeof(m_backtraceAddress) / sizeof(m_backtraceAddress[0]))) break;
        if (!frame.next_pc) break;
        if (!esp_backtrace_get_next_frame(&frame)) break;
    } 

}

void Esp32Backtrace::print(Print& out) const
{
    out.print("py scripts/printSymbols.py");
    for (int i = 0; i < m_depth; ++i) {
        out.printf(" 0x%08x", (uint32_t)m_backtraceAddress[i]);
    }
}

String Esp32Backtrace::toString() const
{
    String result;
    StringPrinter p(result);
    this->print(p);
    return result;
}

