#include <esp_task_wdt.h>                             // ESP32 task watchdog    
#include <Arduino.h>
#include "esp_debug_helpers.h"

// class Watchdog {
// public: 
//     static inline void initialise(int timeoutSeconds = 30) {
//         esp_task_wdt_init(timeoutSeconds, true); // 30 second timeout, panic on trigger
//         esp_task_wdt_add(NULL);      // add current task to WDT watch
//         esp_task_wdt_reset();
//     }
//     static inline  void kickWatchdog() { 
//         esp_task_wdt_reset(); 
//     }
//     static void printRegistrations() {
//   }
// };
