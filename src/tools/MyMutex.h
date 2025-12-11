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
    Esp32Backtrace *m_lockBacktrace = 0;
    String m_name;

    static const int safetyTimeoutMillis = 10000;

  public:
    MyMutex(const String& name);
    bool lock(const char* who, int timeoutMillis = 0);
    void unlock();

  private:
    bool lock(int timeoutMillis = 0);
};

#endif