#include "ThreadSafePtrMap.h"

ThreadSafePtrMap::ThreadSafePtrMap()
{
    m_mutex = xSemaphoreCreateMutex();
}

ThreadSafePtrMap::~ThreadSafePtrMap()
{
    if (m_mutex) {
        vSemaphoreDelete(m_mutex);
        m_mutex = nullptr;
    }
}

void ThreadSafePtrMap::lock() const
{
    xSemaphoreTake(m_mutex, portMAX_DELAY);
}

void ThreadSafePtrMap::unlock() const
{
    xSemaphoreGive(m_mutex);
}

void ThreadSafePtrMap::put(int key, void* ptr)
{
    lock();
    m_map[key] = ptr;
    unlock();
}

void* ThreadSafePtrMap::get(int key) const
{
    lock();
    auto it = m_map.find(key);
    void* out = (it == m_map.end()) ? nullptr : it->second;
    unlock();
    return out;
}

void* ThreadSafePtrMap::take(int key)
{
    lock();
    auto it = m_map.find(key);
    if (it == m_map.end()) {
        unlock();
        return nullptr;
    }
    void* out = it->second;
    m_map.erase(it);
    unlock();
    return out;
}

bool ThreadSafePtrMap::remove(int key)
{
    lock();
    bool removed = (m_map.erase(key) != 0);
    unlock();
    return removed;
}

bool ThreadSafePtrMap::contains(int key) const
{
    lock();
    bool ok = (m_map.find(key) != m_map.end());
    unlock();
    return ok;
}

size_t ThreadSafePtrMap::size() const
{
    lock();
    size_t n = m_map.size();
    unlock();
    return n;
}

void ThreadSafePtrMap::clear()
{
    lock();
    m_map.clear();
    unlock();
}

// In ThreadSafePtrMap.cpp
// ThreadSafePtrMap.cpp
ThreadSafePtrMap::Iterator::Iterator(const ThreadSafePtrMap& map)
    : m_map(map)
{
    m_map.lock();
    m_it  = m_map.m_map.begin();
    m_end = m_map.m_map.end();
}

ThreadSafePtrMap::Iterator::~Iterator() {
    m_map.unlock();
}

bool ThreadSafePtrMap::Iterator::next() {
    if (m_it == m_end) {
        m_hasCurrent = false;
        m_key = -1;
        m_value = nullptr;
        return false;
    }

    m_key = m_it->first;
    m_value = m_it->second;
    m_it++;
    m_hasCurrent = true;
    return true;
}

int ThreadSafePtrMap::Iterator::key() const {
    return m_hasCurrent ? m_key : -1;
}

void* ThreadSafePtrMap::Iterator::value() const {
    return m_hasCurrent ? m_value : nullptr;
}
