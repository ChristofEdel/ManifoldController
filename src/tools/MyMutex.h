#ifndef __MY_MUTEX_H
#define __MY_MUTEX_H

#include <Arduino.h>

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

  public:
    MyMutex(const String& name) : m_name(name)
    {
        m_semaphore = xSemaphoreCreateBinary();
        xSemaphoreGive(m_semaphore);
    }

    bool lock(const char* who, int timeoutMillis = 0)
    {
#ifdef MUTEX_DEBUG
        bool result = lock(timeoutMillis);
        if (result) {
            m_lockHolder = who;
        }
        else {
            Serial.printf("Mutex %s timed out:", this->m_name.c_str());
            Serial.printf("   held by:      %s\n", this->m_lockHolder.c_str());
            Serial.printf("   requested by: %s\n", who);
        }
        return result;
#else
        return lock(timeoutMillis);
#endif
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