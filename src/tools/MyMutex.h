#ifndef __MY_MUTEX_H
#define __MY_MUTEX_H

#include <Arduino.h>

class MyMutex {
  private:
    SemaphoreHandle_t m_semaphore;
  public: 
    MyMutex() {
      m_semaphore = xSemaphoreCreateBinary();
      xSemaphoreGive(m_semaphore);
    }
    bool lock(int timeoutMillis = 0) {
      return xSemaphoreTake(
        m_semaphore, 
        timeoutMillis ? pdMS_TO_TICKS(timeoutMillis) : portMAX_DELAY
      ) == pdTRUE;
    };
    void unlock() {
      xSemaphoreGive(m_semaphore);
    };

    bool lock(const char *message) { 
      // Serial.print("MyMutex::lock   "); Serial.println(message); 
      return lock(); 
    }

    void unlock(const char *message) { 
      // Serial.print("MyMutex::unlock "); Serial.println(message); 
      unlock(); 
    }
};

#endif