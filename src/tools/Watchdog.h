#ifndef __WATCHDOG_H__
#define __WATCHDOG_H__

#include <Arduino.h>

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

class CWatchdogManager {
  public:
    void setup();  // Initialise with NO task watchdogs registered
    void listWatchdogTasks(Print& out);
};

extern CWatchdogManager WatchdogManager;

#endif  // __WATCHDOG_H__