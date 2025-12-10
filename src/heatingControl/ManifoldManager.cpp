#include "ManifoldManager.h"

#include <WiFi.h>

#include "MyLog.h"
#include <lwip/sockets.h>
#include <errno.h>

// Global singleton
CManifoldManager ManifoldManager;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// 
/// ManifoldData - represents the data about one particular manifold
//

// Fill the data from a JSON formatted string. returns false if an error occured
bool ManifoldData::fillFromJson(const String& s)
{
    JsonDocument json;
    DeserializationError error = deserializeJson(json, s);
    if (error) {
        MyLog.print("Failed to parse manifold data file: ");
        MyLog.println(error.c_str());
        return false;
    }
    return this->fillFromJson(json);
}

// Fill the data from a JsonDocument formatted string.
// returns false if an error occured
bool ManifoldData::fillFromJson(JsonDocument& json)
{
    this->name = json["name"] | emptyString;
    this->hostname = json["hostname"] | emptyString;
    this->ipAddress = json["ipAddress"] | emptyString;
    this->on = json["on"].as<bool>();
    this->roomSetpoint = json["roomSetpoint"].as<double>();
    this->roomTemperature = json["roomTemperature"].as<double>();
    this->flowSetpoint = json["flowSetpoint"].as<double>();
    this->flowTemperature = json["flowTemperature"].as<double>();
    this->valvePosition = json["valvePosition"].as<double>();
    this->flowDemand = json["flowDemand"].as<double>();
    this->roomDeltaT = this->roomTemperature - this->roomSetpoint;
    this->flowDeltaT = this->flowTemperature - this->flowSetpoint;
    lastUpdate = time(nullptr);
    // we leave the sequenceNo alone because it is set
    // elsewhere
    return true;
}

// Delivers the data in this object as a JSON  formatted string
String ManifoldData::toJson() const
{
    JsonDocument json;
    json["name"] = this->name;
    json["hostname"] = this->hostname;
    json["ipAddress"] = this->ipAddress;
    json["on"] = this->on;
    json["roomSetpoint"] = this->roomSetpoint;
    json["roomTemperature"] = this->roomTemperature;
    json["flowSetpoint"] = this->flowSetpoint;
    json["flowTemperature"] = this->flowTemperature;
    json["valvePosition"] = this->valvePosition;
    json["flowDemand"] = this->flowDemand;

    String result;
    serializeJson(json, result);
    return result;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// 
/// ManifoldDataPostJob - sending manifold data (fire-and-forget style)
//

QueueHandle_t ManifoldDataPostJob::m_senderQueue = nullptr;      // Queue of FireAndForgetPostJob*
TaskHandle_t ManifoldDataPostJob::m_senderTask = nullptr;  // A task executing these jobs

void ManifoldDataPostJob::post(const ManifoldData& data, String host)
{
    if (host == emptyString) return;
    ensurePostTask();                                               // Ensure the task that posts the jobexists

    // Drain the queue so all pending requests with older data will
    // not be sent
    ManifoldDataPostJob* oldJob = nullptr;
    while (xQueueReceive(m_senderQueue, &oldJob, 0) == pdPASS) delete oldJob;

    // Create a new job and send it to the queue
    ManifoldDataPostJob* job = new ManifoldDataPostJob(host, data);  // Create a job
    BaseType_t ok = xQueueSendToBack(m_senderQueue, &job, 0 /* no wait */);

    if (ok != pdTRUE) {
        delete job;        // queue full or error - ignore, this is "fire and forget"
        return;
    }
}

void ManifoldDataPostJob::ensurePostTask()
{
    // Ensure the post queue exists
    // Queue depth is 1 because we really only want a single post request pending
    // If another newer request is made it will replace the one pending because
    // we only want to send the latest data
    if (!m_senderQueue) {
        m_senderQueue = xQueueCreate(1, sizeof(ManifoldDataPostJob*));
    }

    if (!m_senderTask) {
        xTaskCreatePinnedToCore(
            postTask,
            "PostQueue",
            4096,
            nullptr,
            5,
            &m_senderTask,
            APP_CPU_NUM  // or tskNO_AFFINITY / PRO_CPU_NUM
        );
    }
}

void ManifoldDataPostJob::postTask(void *arg)
{
    ManifoldDataPostJob* job = nullptr;

    for (;;) {
        if (xQueueReceive(m_senderQueue, &job, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (!job) {
            continue;
        }
        if (job->host == emptyString) return;

        if (WiFi.status() == WL_CONNECTED) {
            WiFiClient client;
            client.stop();
            client.setTimeout(1000);
            String body = job->data.toJson();
            // DEBUG_LOG("Sending JSON to %s - %-200s", job->host.c_str(), body.c_str());
            if (client.connect(job->host.c_str(), 80)) {
                const size_t bodyLen = body.length();
                client.print("POST /demand HTTP/1.1");
                client.print("\r\nContent-Type: application/json");
                client.print("\r\nHost: ");
                client.print(job->host);
                client.print("\r\nContent-Length: ");
                client.print(bodyLen);
                client.print("\r\nConnection: close\r\n\r\n");
                client.print(body);
                client.flush();

                // Wait until the response has started
                unsigned long deadline = millis() + 1000;   // 1 second timeout
                while (millis() < deadline && client.connected() && !client.available()) delay(10);

                // String response = "";
                // while (millis() < deadline && client.connected()) {
                //     while (client.available()) {
                //         int c = client.read();
                //         if (c < 0) break;
                //         response += (char) c;
                //     }
                //     delay(10)
                // }
                // DEBUG_LOG("Response: \n%s\n", response.c_str());

                client.stop();
            }
            // else {
            //     DEBUG_LOG("Sending failed, error: %d (%s)\n", errno, strerror(errno));
            // }
        }

        delete job;
    }
}

ManifoldData* CManifoldManager::addOrUpdateManifoldFromJson(const String& s)
{
    JsonDocument json;
    DeserializationError error = deserializeJson(json, s);
    
    if (error) {
        MyLog.print("Failed to parse manifold data file: ");
        MyLog.println(error.c_str());
        return nullptr;
    }
    String name = json["name"].as<String>();
    if (name == emptyString) {
        MyLog.println("Receoived manifold data with no name");
        return nullptr;
    }

    ManifoldData *result = nullptr;
    if (m_manifoldsMutex.lock(__PRETTY_FUNCTION__)) {
        for (auto manifold = m_manifolds.begin(); manifold != m_manifolds.end(); manifold++) {
            if (manifold->name == name) {
                result = &(*manifold);
                break;
            }
        }

        if (!result) {
            m_manifolds.emplace_back();
            result = &m_manifolds.back();
            result->sequenceNo = m_manifolds.size()-1;
        }
        m_manifoldsMutex.unlock();
    }

    result->fillFromJson(json);
    return result;
}

bool CManifoldManager::removeManifold(const String& name)
{
    bool result = false;
    if (m_manifoldsMutex.lock(__PRETTY_FUNCTION__)) {
        for (auto manifold = m_manifolds.begin(); manifold != m_manifolds.end(); manifold++) {
            if (manifold->name == name) {
                m_manifolds.erase(manifold);
                result = true;
                break;
            }
        }
        m_manifoldsMutex.unlock();
    }
    return result;
}

double CManifoldManager::getHighestDemand()
{
    double result = 0;
    if (m_manifoldsMutex.lock(__PRETTY_FUNCTION__)) {
        for (const ManifoldData& manifold : m_manifolds) {
            if (manifold.flowDemand > result) result = manifold.flowDemand;
        }
        m_manifoldsMutex.unlock();
    }
    return result;
}

ManifoldData** CManifoldManager::newSortedCopy()
{
    ManifoldData **result = nullptr;
    int len = 0;

    if (m_manifoldsMutex.lock(__PRETTY_FUNCTION__)) {
        len = m_manifolds.size();
        if (len > 0) {
            result = (ManifoldData**) malloc (sizeof(ManifoldData*) * (len + 1));
            for (size_t i = 0; i < len; i++) {
                result[i] = new ManifoldData(m_manifolds[i]);
            }
            result[m_manifolds.size()] = nullptr;
        }
        m_manifoldsMutex.unlock();
    }

    if (result && len > 2) {
        std::sort(result, result + len, 
            [](const ManifoldData* a, const ManifoldData* b) {
                if (a->name != b->name) return a->name > b->name;
                return a->sequenceNo > b->sequenceNo;
            }
        );
    }
    return result;
}

void CManifoldManager::deleteSortedCopy(ManifoldData** arr)
{
    for (size_t i = 0; arr[i] != nullptr; i++) {
        delete arr[i];       // free each element
    }
    free (arr);
}
