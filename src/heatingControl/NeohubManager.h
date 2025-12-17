#ifndef __NEOHUB_MANAGER_H
#define __NEOHUB_MANAGER_H

#include <ArduinoJson.h>

#include <vector>

#include "NeohubConnection.h"

// A NeohubManager is a global singleton that retrieves and maintains data
// for a neohub using an underlying NeohibConnection.

// Identifier for a zone.
// the Neohub uses primarily names to identify zones, this structure is used to store zone references
// in the configuration so we can (hopefully) deal with renamed zones
struct NeohubZone {
    int id;
    String name;
    NeohubZone(int id, const String& name) : id(id), name(name) {};
    NeohubZone() {};
};

// Data associated with a zone
// (primarily temperature related)
struct NeohubZoneData {
    NeohubZone zone;

    time_t lastUpdate = 0;

    double roomSetpoint = NO_TEMPERATURE;
    double roomTemperature = NO_TEMPERATURE;
    double roomTemperatureSetpoint = NO_TEMPERATURE;
    bool demand = true;

    double floorTemperature = NO_TEMPERATURE;
    bool floorLimitTriggered = false;

    bool online = false;

    static const int NO_TEMPERATURE = -50;

    void clear();
    void storeZoneData(JsonVariant json);

    bool isAged(time_t now) { return now - lastUpdate > 10 && !isDead(now); }
    bool isDead(time_t now) { return now - lastUpdate > 30; }

  private:
    bool found = true;
    friend class CNeohubManager;
};

class CNeohubManager {
  public:
    CNeohubManager() { m_zoneData.reserve(15); };

    // Get a vector with all available zones
    const std::vector<NeohubZoneData>& getZoneData() { ensureZoneNames(); return m_zoneData; };

    // Translation between names and IDs
    String getZoneName(int id);
    int getZoneId(const String& name);

    // Get the data for a particular zone. If it has not been polled,
    // it will be empty except for its name and id
    NeohubZoneData* getZoneData(const String& name, bool forceLoad = false);
    NeohubZoneData* getZoneData(int id, bool forceLoad = false);

    // Get a vector with the  active zones (used for controlling temperature)
    // these may or may not have zone data available in the hub
    const std::vector<NeohubZone>& getActiveZones() { return m_activeZones; }
    void clearActiveZones() { m_activeZones.clear(); };
    void addActiveZone(const NeohubZone& z);
    bool hasActiveZone(int id);

    // Get a vector with the monitored zones (just used for display and in logs)
    // these may or may not have zone data available in the hub
    const std::vector<NeohubZone>& getMonitoredZones() { return m_monitoredZones; };
    void clearMonitoredZones() { m_monitoredZones.clear(); };
    void addMonitoredZone(const NeohubZone& z);
    bool hasMonitoredZone(int id);

    // Lead data for zones from the neohub. Establish the connection
    // if necessary
    void loadZoneDataFromNeohub(bool all = false);
    void loadZoneDataFromNeohub(std::vector<String>& zoneNames);

    // Zone control
    NeohubZoneData* forceZoneSetpoint(String zoneName, double setpoint);
    NeohubZoneData* forceZoneOn(String zoneName);
    NeohubZoneData* forceZoneOff(String zoneName);
    NeohubZoneData* setZoneToAutomatic(String zoneName);

    // Direct connection to the neohub
    bool ensureNeohubConnection();
    void reconnect();
    String neohubCommand(const String& command, int timeoutMillis = commandTimeoutMillis);

    static const int connectTimeoutMillis = 5000;
    static const int commandTimeoutMillis = 2000;

  private:
    // Connecetion to Neohub
    NeohubConnection* m_connection = nullptr;

    // Vector with the data for all zones
    std::vector<NeohubZoneData> m_zoneData;
    NeohubZoneData* getOrCreateZoneData(int id, const String& name);
    void ensureZoneNames();
    void loadZoneNames();

    std::vector<NeohubZone> m_activeZones;
    std::vector<NeohubZone> m_monitoredZones;

    // loop function which regularly ensures the connection and polls data as requied
    void loop();

    // The loop task which regularly executes the loop function
    TaskHandle_t m_loopTaskHandle = nullptr;
    void ensureLoopTask();
    static void loopTask(void* parameter);

    // Mutex to avoid parallel use by separate tasks
    MyMutex m_neohubMutex = MyMutex("CNeohubManager::m_neohubMutex");
};

extern CNeohubManager NeohubManager;

#endif