#ifndef __THREAD_SAFE_PTR_MAP_H
#define __THREAD_SAFE_PTR_MAP_H

#include <unordered_map>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class ThreadSafePtrMap {
  private:
    mutable SemaphoreHandle_t m_mutex;
    mutable std::unordered_map<int, void*> m_map;

  public:
    ThreadSafePtrMap();
    ~ThreadSafePtrMap();

    void put(int key, void* ptr);  // Add a pointer to the map (or replace if it exists    )
    void* get(int key) const;      // Get a pointer from the map (nullptr if not found)
    void* take(int key);           // Get and remove a pointer from the map (nullptr if not found)
    bool remove(int key);          // Remove a pointer from the map (returns true if found and removed)
    bool contains(int key) const;  // Check if the map contains a key
    size_t size() const;           // Get the number of entries in the map
    void clear();                  // Clear all entries from the map

    class Iterator {               // Iterator for the map (thread safe, map locked until destruction)
      public:
        explicit Iterator(const ThreadSafePtrMap& map);
        ~Iterator();

        bool  next();           // advances; returns false when done
        int   key() const;     // current key (valid after next()==true)
        void* value() const;   // current value (valid after next()==true)
        bool next(int& key, void*& value);

      private:
        const ThreadSafePtrMap& m_map;
        std::unordered_map<int, void*>::iterator m_it;
        std::unordered_map<int, void*>::iterator m_end;
        int   m_key = -1;
        void* m_value = nullptr;
        bool  m_hasCurrent = false;
    };

  private:
    void lock() const;
    void unlock() const;
};

#endif