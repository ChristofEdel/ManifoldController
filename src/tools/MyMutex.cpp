#include <MyMutex.h>

MyMutex::MyMutex(const String& name) : m_name(name)
{
    m_semaphore = xSemaphoreCreateBinary();
    xSemaphoreGive(m_semaphore);
}

bool MyMutex::lock(const char* who, int timeoutMillis /* = 0 */)
{
    bool result;

    // If no timeout is specified, we will time out at the
    // "safety timeout" and abort
    if (!timeoutMillis) {
        unsigned long startMillis = millis();
        result = lock(safetyTimeoutMillis);
        if (result) {
            m_lockHolder = who;
            m_lockBacktrace = new Esp32Backtrace(1);
            return result;
        }
        Esp32Backtrace backtrace(0);
        softwareAbort(
            SW_RESET_MUTEX_TIMEOUT,
            "MyMutex(%s):\n    locked by: %s\n               %s\n    failed in: %s\n               %s\n    after %d ms",
            m_name.c_str(),
            m_lockHolder.c_str(),
            m_lockBacktrace ? m_lockBacktrace->toString().c_str() : "NO BACKTRACE???",
            who,
            backtrace.toString().c_str(),
            millis() - startMillis);
    }

    // Otherwise, we will assume this is what the calling
    // code wanted and that it will handle the timeout
    result = lock(timeoutMillis);
    if (result) {
        m_lockHolder = who;
        m_lockBacktrace = new Esp32Backtrace(1);
    }
    return result;
}

void MyMutex::unlock()
{
    m_lockHolder = "";
    delete m_lockBacktrace;
    m_lockBacktrace = 0;
    xSemaphoreGive(m_semaphore);
}

bool MyMutex::lock(int timeoutMillis /* = 0 */)
{
    return xSemaphoreTake(
               m_semaphore,
               timeoutMillis ? pdMS_TO_TICKS(timeoutMillis) : MUTEX_DEFAULT_TIMEOUT) == pdTRUE;
};
