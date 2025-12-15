#include "Watchdog.h"

#include <Arduino.h>
#include <esp_task_wdt.h>  // ESP32 task watchdog
#include "freertos/task.h"

// Global singleton
CWatchdogManager WatchdogManager;

void CWatchdogManager::setup()
{
    // esp_task_wdt_print_triggered_tasks(0,0,0);

    Serial.println("Our opinion:");
    listWatchdogs(Serial);

    Serial.println("Clearing.");
    TaskHandle_t idle0 = xTaskGetIdleTaskHandleForCore(0);
    TaskHandle_t idle1 = xTaskGetIdleTaskHandleForCore(1);

    // Get all tasks
    UBaseType_t n = uxTaskGetNumberOfTasks();
    TaskStatus_t* ts = (TaskStatus_t*)malloc(n * sizeof(TaskStatus_t));
    n = uxTaskGetSystemState(ts, n, nullptr);

    // Deregister them from the WDT if they are registered
    for (UBaseType_t i = 0; i < n; ++i) {
        if (ts[i].xHandle == idle0 || ts[i].xHandle == idle1) {
            // Skip idle tasks
            continue;
        }
        if (esp_task_wdt_status(ts[i].xHandle) == ESP_OK) {
            esp_err_t result = esp_task_wdt_delete(ts[i].xHandle);
            Serial.printf(" - Deregistered task %s from WDT, result = %d\n", ts[i].pcTaskName, result);
        }
    }
    free(ts);
    
    // Deinitialise the WDT
    Serial.println("Deinit");
    esp_task_wdt_deinit();

    Serial.println("Our opinion:");
    listWatchdogs(Serial);

}

void CWatchdogManager::listWatchdogs(Print& out)
{
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