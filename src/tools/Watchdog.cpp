#include "Watchdog.h"

#include <Arduino.h>
#include <esp_task_wdt.h>  // ESP32 task watchdog
#include "freertos/task.h"
#include "esp_freertos_hooks.h"

// Global singleton
CWatchdogManager WatchdogManager;

void CWatchdogManager::setup()
{

    // esp_task_wdt_config_t cfg = {
    //     .timeout_ms     = 1000, // 1 second timeout, we don't really want it
    //     .idle_core_mask = 0,    // disables idle task checking on all cores
    //     .trigger_panic  = true  // Really should not trigger but if it does we wabt a core dump
    // };

    // esp_task_wdt_reconfigure(&cfg);

    // Get all tasks
    // UBaseType_t n = uxTaskGetNumberOfTasks();
    // TaskStatus_t* ts = (TaskStatus_t*)malloc(n * sizeof(TaskStatus_t));
    // n = uxTaskGetSystemState(ts, n, nullptr);
    // TaskHandle_t idle0 = xTaskGetIdleTaskHandleForCore(0);
    // TaskHandle_t idle1 = xTaskGetIdleTaskHandleForCore(1);

    // // Deregister them from the WDT if they are registered AND if they are not the Idle tasks
    // for (UBaseType_t i = 0; i < n; ++i) {
    //     if (ts[i].xHandle == idle0 || ts[i].xHandle == idle1) {
    //         // Skip idle tasks
    //         continue;
    //     }
    //     if (esp_task_wdt_status(ts[i].xHandle) == ESP_OK) {
    //         esp_err_t result = esp_task_wdt_delete(ts[i].xHandle);
    //         Serial.printf(" - Deregistered task %s from WDT, result = %d\n", ts[i].pcTaskName, result);
    //     }
    // }
    // free(ts);

    // Deinitilise the  WDT
    // This also deregisters the Idle task watchdogs, but properly so 
    // they no longer call edt_reset() in their idle hooks
    // esp_task_wdt_deinit();

}

void CWatchdogManager::listWatchdogTasks(Print& out)
{
    // esp_task_wdt_print_triggered_tasks(0,0,0);
    // The function above lists tasks that have not yet reset the watchdog
    // and intended to be called once it HAS actually triggered.
    // We want to list all registered tasks, so we have to do it ourselves

    // Get all tasks
    UBaseType_t n = uxTaskGetNumberOfTasks();
    TaskStatus_t* ts = (TaskStatus_t*)malloc(n * sizeof(TaskStatus_t));
    n = uxTaskGetSystemState(ts, n, nullptr);

    // Initial checks - how many registered tasks are there?
    int registrationCount = 0;
    int notInitialisedCount = 0;

    for (UBaseType_t i = 0; i < n; ++i) {
        esp_err_t result = esp_task_wdt_status(ts[i].xHandle);
        switch (result) {
            case ESP_OK:
                registrationCount++;
                break;
            case ESP_ERR_NOT_FOUND:
                // not registered
                break;
            case ESP_ERR_INVALID_STATE:
                notInitialisedCount++;
                break;
            default:
                out.printf("esp_task_wdt_status returned unexpected value for task %s: %d\n", ts[i].pcTaskName, result);
                break;
        };
    }

    // If there are any tasks reporting INVALID_STATE, wer report that the
    // watchdog is not initialised
    if (notInitialisedCount) {
        out.println("Task Watchdog Timer not initialised");
    }

    // We then print all registered tasks
    // wo do that even if we have notInitialisedCount although it should never happen
    // because IF it happens we would want to know which tasks are registered
    if (registrationCount) {
        out.println("Tasks registered with Task Watchdog Timer:");
        for (UBaseType_t i = 0; i < n; ++i) {
            if (esp_task_wdt_status(ts[i].xHandle) == ESP_OK) {
                out.printf(" - %s\n", ts[i].pcTaskName);
            }
        }
        if (notInitialisedCount) {
            out.printf("\nesp_task_wdt_status returned ESP_ERR_INVALID_STATE for %d tasks:\n", notInitialisedCount);
            for (UBaseType_t i = 0; i < n; ++i) {
                if (esp_task_wdt_status(ts[i].xHandle) == ESP_ERR_INVALID_STATE) {
                    out.printf(" - %s\n", ts[i].pcTaskName);
                }
            }
        }
    }
    else {
        out.println("No tasks are registered with the Task Watchdog Timer.");
    }

    free (ts);
    return;
}