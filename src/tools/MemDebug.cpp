#include "MemDebug.h"

#include <Arduino.h>
#include <esp_system.h>  // For esp_get_free_heap_size()

#include "EspTools.h"
#include "MyLog.h"
#include "esp_heap_caps.h"

extern "C" void heap_caps_alloc_failed_hook(
    size_t requested_size,
    uint32_t caps,
    const char* function_name
)
{
    softwareAbort(SW_RESET_OUT_OF_MEMORY);
}

uint32_t freeRam()
{
    return esp_get_free_heap_size();
}

uint32_t ramAvailable = 0;

void setupMemDebug()
{
    heap_caps_register_failed_alloc_callback(heap_caps_alloc_failed_hook);
}

void checkMemoryChange(bool forceOutput, int thresholdBytes /* = 1024 */)
{
    checkMemoryChange(nullptr, forceOutput, thresholdBytes);
}

void checkMemoryChange(char* message, bool forceOutput /* = true */, int thresholdBytes /* = 1024 */)
{
    uint32_t newRamAvailable = freeRam();
    if (newRamAvailable < (ramAvailable - thresholdBytes) || forceOutput) {
        MyLog.print("Memory available on heap");
        if (message) {
            MyLog.print(" (");
            MyLog.print(message);
            MyLog.print(")");
        }
        MyLog.print(": ");
        MyLog.print(newRamAvailable);
        if (newRamAvailable < (ramAvailable - thresholdBytes) && ramAvailable != 0) {
            MyLog.print(" (");
            MyLog.print((int32_t)(newRamAvailable - ramAvailable));
            MyLog.print(")");
        }
        MyLog.println();
        ramAvailable = newRamAvailable;
    }
}

void checkMemoryChange(int thresholdBytes /* = 1024 */)
{
    checkMemoryChange(nullptr, false, thresholdBytes);
}
