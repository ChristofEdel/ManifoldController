#ifndef __MY_MUTEX_H
#define __MY_MUTEX_H

#include <Arduino.h>
#include "EspTools.h"

// #define MUTEX_DEBUG
// #define MUTEX_DEFAULT_TIMEOUT  pdMS_TO_TICKS(10000)
#ifndef MUTEX_DEFAULT_TIMEOUT
#define MUTEX_DEFAULT_TIMEOUT portMAX_DELAY
#endif

class MyMutex {
  private:
    SemaphoreHandle_t m_semaphore;
    String m_lockHolder;
    String m_name;

    static const int safetyTimeoutMillis = 10000;

  public:
    MyMutex(const String& name) : m_name(name)
    {
        m_semaphore = xSemaphoreCreateBinary();
        xSemaphoreGive(m_semaphore);
    }

    bool lock(const char* who, int timeoutMillis = 0)
    {
        bool result;

        // If no timeout is specified, we will time out at the
        // "safety timeout" and abort
        if (!timeoutMillis) {
            unsigned long startMillis = millis();
            result = lock(safetyTimeoutMillis);
            if (result) {
                m_lockHolder = who;
                return result;
            }
            softwareAbort(
                SW_RESET_MUTEX_TIMEOUT, 
                "MyMutex(%s):\n    locked by: %s\n    failed in:  %s\n    after %d ms",
                m_name.c_str(),
                m_lockHolder.c_str(),
                who,
                millis() - startMillis
            );
        }

        // Otherwise, we will assume this is what the calling
        // code wanted and that it will handle the timeout
        result = lock(timeoutMillis);
        if (result) {
            m_lockHolder = who;
        }
        return result;
    }

    void unlock()
    {
        xSemaphoreGive(m_semaphore);
        m_lockHolder = "";
    }

  private:
    bool lock(int timeoutMillis = 0)
    {
        return xSemaphoreTake(
            m_semaphore,
            timeoutMillis ? pdMS_TO_TICKS(timeoutMillis) : MUTEX_DEFAULT_TIMEOUT
        ) == pdTRUE;
    };
};

#endif