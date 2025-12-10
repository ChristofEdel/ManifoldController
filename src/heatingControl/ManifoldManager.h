#ifndef __MANIFOLD_MANAGER_H
#define __MANIFOLD_MANAGER_H

#include <Arduino.h>

#include <vector>

#include "ArduinoJson.h"
#include "MyMutex.h"
#include "freertos/queue.h"
#include "freertos/task.h"

struct ManifoldData {
  public:
    String name;
    String hostname;
    String ipAddress;
    int sequenceNo;
    bool on;
    double roomSetpoint;
    double roomTemperature;
    double roomDeltaT;
    double flowSetpoint;
    double flowTemperature;
    double flowDeltaT;
    double valvePosition;
    double flowDemand;
    time_t lastUpdate;

    bool fillFromJson(const String& s);
    bool fillFromJson(JsonDocument& json);
    String toJson() const;

    bool isAged(time_t now) { return now - lastUpdate > 30 && !isDead(now); }
    bool isDead(time_t now) { return now - lastUpdate > 120; }

};

struct ManifoldDataPostJob {
    String host;
    ManifoldData data;

    static void post(const ManifoldData& data, String host);

  private:
    ManifoldDataPostJob(String host, const ManifoldData& data) : host(host), data(data) {}
    ManifoldDataPostJob(){};
    static void ensurePostTask();
    static void postTask(void* arg);
    static QueueHandle_t m_senderQueue;
    static TaskHandle_t m_senderTask;
};

// OneWireManager is a class giving access to one OneWireTemperatureSensor struct
// for every sensor. The constructor takes the I/O pin for the OneWire bus as parameter
class CManifoldManager {
  private:
    std::vector<ManifoldData> m_manifolds;
    MyMutex m_manifoldsMutex;

  public:
    CManifoldManager() : m_manifoldsMutex("ManifoldManager") { m_manifolds.reserve(6); }

    int getCount() { return m_manifolds.size(); }

    ManifoldData* addOrUpdateManifoldFromJson(const String&);
    bool removeManifold(const String& name);
    double getHighestDemand();

    ManifoldData** newSortedCopy();
    void deleteSortedCopy(ManifoldData** arr);

};

extern CManifoldManager ManifoldManager;

#endif