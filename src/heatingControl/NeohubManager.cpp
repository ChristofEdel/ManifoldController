#include "NeohubManager.h"
#include "MyLog.h"

extern CNeohubManager NeohubManager;

NeohubZoneData * CNeohubManager::getZoneData(const String &name) {
    if (this->m_zoneData.empty()) loadZoneNames();
    
    // Find the matching entry in the zone list
    NeohubZoneData * result = nullptr;
    for (NeohubZoneData & data: this->m_zoneData) {
        if (data.zone.name == name) return &data;
    }
    return result;
}

NeohubZoneData * CNeohubManager::getZoneData(int id) {
    if (this->m_zoneData.empty()) loadZoneNames();
    
    // Find the matching entry in the zone list
    NeohubZoneData * result = nullptr;
    for (NeohubZoneData & data: this->m_zoneData) {
        if (data.zone.id == id) return &data;
    }
    return result;
}

bool CNeohubManager::loadFromNeohub(NeohubZoneData *data) {
    if (!ensureNeohubConnection()) return false;
    return false;
}

bool CNeohubManager::ensureNeohubConnection() {

    // we are connected --> done
    if (this->m_connection && this->m_connection->isConnected()) return true;

    // Create connection object if necessary
    if (!this->m_connection) {
        this->m_connection = new NeohubConnection(this->m_url, this->m_token);
    }

    // Connect
    MyLog.printf("Connecting to NeoHub %s\n", this->m_url.c_str());
    this->m_connection->onConnect([this]() {
        MyLog.printf("Connection to NeoHub %s established\n", this->m_url.c_str());
    });
    this->m_connection->onDisconnect([this]() {
        MyLog.printf("Connection to NeoHub %s disconnected\n", this->m_url.c_str());
        this->m_connection->finish();
        this->m_connection = nullptr;
    });
    this->m_connection->onError([this](String message) {
        MyLog.printf("Error in NeoHub %s connection: %s\n", this->m_url.c_str(), message.c_str());
    });
    this->m_connection->open();

    // Wait for connecetion
    unsigned long startMillis = millis();
    while (
        millis() - startMillis < connectTimeoutMillis           // until timeout
        && !this->m_connection->isConnected()                   // or until connected
    ) delay(100);

    if (!this->m_connection->isConnected()) {
        MyLog.printf("Establishing connection to Neohub %s failed\n", this->m_url.c_str());
        this->m_connection = nullptr;
        return false;
    }

    return true;
}

String CNeohubManager::neohubCommand(const String & command, int timeoutMillis /* = commandTimeoutMillis */) {
    if (!ensureNeohubConnection()) return emptyString;

    bool done = false;
    bool error = false;
    String result;

    DEBUG_LOG ("Sending command: %s", command.c_str());
    this->m_connection->send(
        command,
        [&done, &result](String response) {
            result = response;
            done = true;
        },
        [&error, &result](String message) {
            result = message;
            error = true;
        },
        timeoutMillis
    );

    // Wait for response
    unsigned long startMillis = millis();
    timeoutMillis += 100; // extra 100ms grace so we don't time out before the send above does
    while (
        millis() - startMillis < timeoutMillis          // until timeout
        && !done && !error                              // or until completed
    ) delay(100);

    if (error) {
        MyLog.printf("Error when waiting for Neohub response: %s\n", result.c_str());
        MyLog.printf("Command was '%s'\n", command.c_str());
        return emptyString;
    }
    if (!done) {
        MyLog.printf("Timeout when waiting for Neohub response: %s\n", result.c_str());
        MyLog.printf("Command was '%s'\n", command.c_str());
        return emptyString;
    }
    DEBUG_LOG ("Response: %s", result.c_str());
    return result;
}

void CNeohubManager::ensureZoneNames() {
    if (m_zoneData.empty()) {
        loadZoneNames();
    }
}

void CNeohubManager::loadZoneNames() {
    if (!ensureNeohubConnection()) return;

    String response = neohubCommand("{ 'GET_ZONES': 0 }");
    JsonDocument json;
    DeserializationError error = deserializeJson(json, response);
    if (error) {
        MyLog.printf("Failed to deserialise JSON for GET_ZONES: %s\n", error.c_str());
        return;
    }

    JsonObjectConst obj = json.as<JsonObjectConst>();

    for (NeohubZoneData & data: this->m_zoneData) data.found = false;

    for (JsonPairConst line : obj) {
        String name = line.key().c_str();
        int id = line.value().as<int>();
        NeohubZoneData *data = this->getOrCreateZoneData(id, name);
        data->found = true;
    }
    for (auto iterator = this->m_zoneData.begin(); iterator != this->m_zoneData.end(); ) {
        if (!iterator->found) {
            iterator = this->m_zoneData.erase(iterator);
        }
        else {
            iterator++;
        }
    }
}

NeohubZoneData * CNeohubManager::getOrCreateZoneData(int id, const String &name){

    for (NeohubZoneData & existingData: this->m_zoneData) {
        if (existingData.zone.id == id) return &existingData;
    }
    this->m_zoneData.emplace_back();
    NeohubZoneData *result = &this->m_zoneData.back();
    result->zone.id = id;
    result->zone.name = name;
    return result;
}

void CNeohubManager::loadAllFromNeohub(bool flaggedAsPollInLoopOnly /* = false */) {
    this->ensureNeohubConnection();
    for(NeohubZoneData &stat : this->m_zoneData) {
        if (flaggedAsPollInLoopOnly && !stat.pollInLoop) continue;
        loadFromNeohub(&stat);
    }
}

void CNeohubManager::addActiveZone(const NeohubZone &z) {
    for (NeohubZone az: m_activeZones) {
        if(az.id == z.id && az.name == z.name) return;  // already there
    }
    m_activeZones.emplace_back(z);
}
bool CNeohubManager::hasActiveZone(int id) {
    for (NeohubZone az: m_activeZones) {
        if(az.id == id) return true;
    }
    return false;
}


void CNeohubManager::addMonitoredZone(const NeohubZone &z) {
    for (NeohubZone az: m_monitoredZones) {
        if(az.id == z.id && az.name == z.name) return;  // already there
    }
    m_monitoredZones.emplace_back(z);
}
bool CNeohubManager::hasMonitoredZone(int id) {
    for (NeohubZone az: m_monitoredZones) {
        if(az.id == id) return true;
    }
    return false;
}

void CNeohubManager::loop() {

    const unsigned long pollInterval      = 5000;     // How often we poll the zones (ms)
    const unsigned long connectInterval   = 30000;    // How often we make sure the connection exists (ms)
                                                      // Note that polling also ensures the connection
                                                      // exists so this is for when no polling happens

    static unsigned long lastPoll = 0;
    static unsigned long lastConnectionCheck = 0;
    static bool first = true;

    unsigned long timeNow = millis();
    if (first || timeNow - lastConnectionCheck >= connectInterval) {
        this->ensureNeohubConnection();
        lastConnectionCheck = timeNow;
    }

    if (first || timeNow - lastPoll >= pollInterval) {
        this->loadAllFromNeohub(/* flaggedAsPollInLoopOnly: */ true);
        lastPoll = timeNow;
    }

    // use delay so we don't use too much CPU going round and round in circles...
    delay (100);
}

void CNeohubManager::loopTask(void *parameter) {
    CNeohubManager *_this = (CNeohubManager *) parameter;
    for (;;) _this->loop();
}

void CNeohubManager::ensureLoopTask() {
    if (this->m_loopTaskHandle) return;
    xTaskCreate(
        CNeohubManager::loopTask, // Task function
        "NeohubLoop",             // Task name
        4096,                     // Stack size (bytes)
        this,                     // Parameter to pass
        1,                        // Task priority
        &this->m_loopTaskHandle   // Task handle
    );
    
}

CNeohubManager NeohubManager;



