#ifndef __SENROR_MAP_H
#define __SENROR_MAP_H
#include <SdFat.h>
#include "OneWireManager.h"

struct SensorMapEntry {
    String id;    // Ths ID used by the OneWireManager for this sensor
    String name;  // The display name given to this sensor, configured by the user
    int index;    // The index of this sensor in the sequence of sensors, starting with 0
    SensorMapEntry(const String& id, const String& name, int index) : id(id), name(name), index(index) {};
};

class SensorMap {
  private:
  public:
    SensorMapEntry** m_sensorMapStorage;

  private:
    int m_maxSensorCount;
    int m_sensorCount;
    bool m_changed;

  public:
    SensorMap(int maxSensorCount);
    ~SensorMap();
    bool save(SdFs* fs, const char* fileName);
    bool load(SdFs* fs, const char* fileName);
    int getCount() const { return m_sensorCount; }
    inline SensorMapEntry* operator[](int index) const { return m_sensorMapStorage[index]; }
    inline SensorMapEntry* operator[](const String& id) const { return this->findEntryById(id); }

    const String& getNameForId(const String& id) const;
    const String& getIdForName(const String& name) const;
    void setNameForId(const String& id, const String& name);
    void updateAtIndex(int index, const String& id, const String& name);

    void removeId(const String& id);
    void removeAtindex(int index);
    void removeFromIndex(int index);

    void dump(Print& p) const;
    void clear();
    bool hasChanged() const { return m_changed; };
    void clearChanged() { m_changed = false; };

  private:
    SensorMapEntry* findEntryByName(const String& name) const;
    SensorMapEntry* findEntryById(const String& id) const;
    void removeEntry(SensorMapEntry* ep);
};

#endif