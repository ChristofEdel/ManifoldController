#include "NeohubManager.h"

#include "MyConfig.h"
#include "MyLog.h"
#include "StringPrintf.h"

int NeohubConnection::nextInstanceNo = 100;


// Store the data obtained from the Neohub in form of a JSON object
// in this NeohubZoneData object
void NeohubZoneData::storeZoneData(JsonVariant obj)
{
    this->online = !obj["OFFLINE"].as<bool>();
    if (!this->online) {
        this->clear();
    }
    else {
        this->roomSetpoint = obj["CURRENT_SET_TEMPERATURE"].as<float>();
        this->roomTemperature = obj["CURRENT_TEMPERATURE"].as<float>();
        String s = obj["CURRENT_TEMPERATURE"].as<String>();
        this->roomTemperatureSetpoint = obj["CURRENT_SET_TEMPERATURE"].as<float>();
        this->demand = obj["HEATING"].as<bool>();

        this->floorTemperature = obj["CURRENT_FLOOR_TEMPERATURE"].as<float>();
        if (this->floorTemperature > 100 || this->floorTemperature < 0) {
            this->floorTemperature = NeohubZoneData::NO_TEMPERATURE;
            this->floorLimitTriggered = false;
        }
        else {
            this->floorLimitTriggered = obj["FLOOR_LIMIT"].as<bool>();
        }
    }
}

// Clear the data and set it to "no data" values
void NeohubZoneData::clear()
{
    roomSetpoint = NO_TEMPERATURE;
    roomTemperature = NO_TEMPERATURE;
    roomTemperatureSetpoint = NO_TEMPERATURE;
    demand = true;

    floorTemperature = NO_TEMPERATURE;
    floorLimitTriggered = false;

    online = false;
}

String CNeohubManager::getZoneName(int id) {
    NeohubZoneData* zd = getZoneData(id);
    if (!zd) return emptyString;
    return zd->zone.name;
}

int CNeohubManager::getZoneId(const String& name) {
    NeohubZoneData* zd = getZoneData(name);
    if (!zd) return -1;
    return zd->zone.id;
}

// Get the data for the zone with the given name (if it exits)
NeohubZoneData* CNeohubManager::getZoneData(const String& name, bool forceLoad /* = false */)
{
    if (this->m_zoneData.empty()) loadZoneNames();
    if (forceLoad) this->loadZoneDataFromNeohub(name);

    // Find the matching entry in the zone list
    NeohubZoneData* result = nullptr;
    for (NeohubZoneData& data : this->m_zoneData) {
        if (data.zone.name == name) return &data;
    }
    return result;
}

// Get the data for the zone with the given id (if it exits)
NeohubZoneData* CNeohubManager::getZoneData(int id, bool forceLoad /* = false */)
{
    if (this->m_zoneData.empty()) loadZoneNames();
    if (forceLoad) {
        String name = this->getZoneName(id);
        if (name != emptyString) this->loadZoneDataFromNeohub(id);
    }

    // Find the matching entry in the zone list
    NeohubZoneData* result = nullptr;
    for (NeohubZoneData& data : this->m_zoneData) {
        if (data.zone.id == id) return &data;
    }
    return result;
}


// Force the zone to the fgiven setpoint for 5 minutes
NeohubZoneData* CNeohubManager::forceZoneSetpoint(String zoneName, double setpoint) {
    String command = StringPrintf(
        "{'HOLD': [ {'temp':%.1f, 'hours':0, 'minutes':5, 'id':'Force %s'}, ['%s']]}",
        setpoint, zoneName, zoneName
    );
    String result = neohubCommand(command);
    this->loadZoneDataFromNeohub(zoneName);
    return this->getZoneData(zoneName);
}

// Force the zone on for 5 minutes by setting a high stpoint.
// Returns the setpoint for the zone, or NeohubZoneData::NO_TEMPERATURE
NeohubZoneData* CNeohubManager::forceZoneOn(String zoneName)
{
    return forceZoneSetpoint(zoneName, 35.0);
}

// Force the zone off for 5 minutes by setting a low setpoint.
// Returns the setpoint for the zone
NeohubZoneData* CNeohubManager::forceZoneOff(String zoneName)
{
    return forceZoneSetpoint(zoneName, 15.0);
}

// Cancel the forced setpoint for the zone
NeohubZoneData* CNeohubManager::setZoneToAutomatic(String zoneName)
{
    String command = StringPrintf("{'CANCEL_HGROUP': 'Force %s'}", zoneName);
    String result = neohubCommand(command);
    this->loadZoneDataFromNeohub(zoneName);
    return this->getZoneData(zoneName);
}

// Ensure that the connection to the NeoHub exists.
bool CNeohubManager::ensureNeohubConnection()
{
    static bool noUrlReported = false;
    static bool noTokenReported = false;

    const String& url = Config.getNeohubAddress();
    const String& token = Config.getNeohubToken();
    if (url == "null" || url == "") {
        if (!noUrlReported) {
            MyLog.printf("Unable to connecet  to NeoHub - no URL: %s\n", url.c_str());
            noUrlReported = true;
        }
        return false;
    }
    if (token == "null" || token == "") {
        if (!noTokenReported) {
            MyLog.printf("Unable to connecet to NeoHub - no token: %s\n", url.c_str());
            noTokenReported = true;
        }
        return false;
    }

    if (m_neohubMutex.lock(__PRETTY_FUNCTION__)) {
        // we are connected --> done
        if (this->m_connection && this->m_connection->isConnected()) {
            m_neohubMutex.unlock();
            noUrlReported = false;
            noTokenReported = false;
            return true;
        }

        // Create connection object if necessary
        if (!this->m_connection) {
            this->m_connection = new NeohubConnection(url, token);
        }

        // Connect
        MyLog.printf("Connecting to NeoHub %s\n", url.c_str());
        this->m_connection->onConnect([this, url]() {
            MyLog.printf("Connection to NeoHub %s established\n", url.c_str());
        });
        this->m_connection->onDisconnect([this, url]() {
            MyLog.printf("Connection to NeoHub %s disconnected\n", url.c_str());
            NeohubConnection* c = this->m_connection;
            this->m_connection = nullptr;
            c->finish();
        });
        this->m_connection->onError([this, url](String message) {
            MyLog.printf("Error in NeoHub %s connection: %s\n", url.c_str(), message.c_str());
        });
        this->m_connection->connect();

        // Wait for connecetion
        unsigned long startMillis = millis();
        while (
            millis() - startMillis < connectTimeoutMillis  // until timeout
            && this->m_connection                          // or until deleted, e.g., by disconnect after error
            && !this->m_connection->isConnected()          // or until connected
        ) delay(100);

        m_neohubMutex.unlock();
    }

    if (!this->m_connection || !this->m_connection->isConnected()) {
        MyLog.printf("Establishing connection to Neohub %s failed\n", url.c_str());
        this->m_connection = nullptr;
        return false;
    }

    ensureLoopTask();

    return true;
}

// Disconnect (if necessary) from the Neohub end reestablish the connection
void CNeohubManager::reconnect()
{
    if (m_neohubMutex.lock(__PRETTY_FUNCTION__)) {
        if (this->m_connection && this->m_connection->isConnected()) {
            this->m_connection->disconnect();
            // Disconnected event takes care of clean up, do NOT delete here
        }
        m_neohubMutex.unlock();
    }
    ensureNeohubConnection();
}

// Send a command to the Neohub and synchronously wait for the respnse
String CNeohubManager::neohubCommand(const String& command, int timeoutMillis /* = commandTimeoutMillis */)
{
    if (!ensureNeohubConnection()) return emptyString;

    bool done = false;
    bool error = false;
    String result;

    if (m_neohubMutex.lock(__PRETTY_FUNCTION__)) {
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
            timeoutMillis);

        // Wait for response
        unsigned long startMillis = millis();
        timeoutMillis += 100;  // extra 100ms grace so we don't time out before the send above does
        while (
            millis() - startMillis < timeoutMillis  // until timeout
            && !done && !error                      // or until completed
            ) delay(100);
        m_neohubMutex.unlock();
    }

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

    return result;
}

void CNeohubManager::ensureZoneNames()
{
    if (m_zoneData.empty()) {
        loadZoneNames();
    }
}

// Load the names and ids of all zones from the Neohub
void CNeohubManager::loadZoneNames()
{
    if (!ensureNeohubConnection()) return;

    String response = neohubCommand("{ 'GET_ZONES': 0 }");
    JsonDocument json;
    DeserializationError error = deserializeJson(json, response);
    if (error) {
        MyLog.printf("Failed to deserialise JSON for GET_ZONES: %s\n", error.c_str());
        return;
    }

    JsonObjectConst obj = json.as<JsonObjectConst>();

    for (NeohubZoneData& data : this->m_zoneData) data.found = false;

    for (JsonPairConst line : obj) {
        String name = line.key().c_str();
        int id = line.value().as<int>();
        NeohubZoneData* data = this->getOrCreateZoneData(id, name);
        data->found = true;
    }
    for (auto iterator = this->m_zoneData.begin(); iterator != this->m_zoneData.end();) {
        if (!iterator->found) {
            iterator = this->m_zoneData.erase(iterator);
        }
        else {
            iterator++;
        }
    }
}

// Find a zone with the given Id, or add it if it already exists
NeohubZoneData* CNeohubManager::getOrCreateZoneData(int id, const String& name)
{
    for (NeohubZoneData& existingData : this->m_zoneData) {
        if (existingData.zone.id == id) return &existingData;
    }
    this->m_zoneData.emplace_back();
    NeohubZoneData* result = &this->m_zoneData.back();
    result->zone.id = id;
    result->zone.name = name;
    return result;
}

// Add a zone to the list of the "active" zones, which are zones
// used for temperature control
void CNeohubManager::addActiveZone(const NeohubZone& z)
{
    for (NeohubZone az : m_activeZones) {
        if (az.id == z.id && az.name == z.name) return;  // already there
    }
    m_activeZones.emplace_back(z);
}

// Check if the zone with the given id is an "active" zone
bool CNeohubManager::hasActiveZone(int id)
{
    for (NeohubZone az : m_activeZones) {
        if (az.id == id) return true;
    }
    return false;
}

// Add a zone to the list of zones which are monitored (display only)
void CNeohubManager::addMonitoredZone(const NeohubZone& z)
{
    for (NeohubZone az : m_monitoredZones) {
        if (az.id == z.id && az.name == z.name) return;  // already there
    }
    m_monitoredZones.emplace_back(z);
}

// Check if the zone with the given id is a monitored zone
bool CNeohubManager::hasMonitoredZone(int id)
{
    for (NeohubZone az : m_monitoredZones) {
        if (az.id == id) return true;
    }
    return false;
}


// Load the data for the active and monitored zones from the nehub
// (or for all zones if the parameter all is set to ture)
void CNeohubManager::loadZoneDataFromNeohub(bool all /* = false */)
{
    this->ensureNeohubConnection();

    std::vector<String> zoneNames;
    if (all) {
        for (NeohubZoneData& d : this->m_zoneData) {
            zoneNames.push_back(d.zone.name);
        }
    }
    else {
        for (NeohubZone& z : this->m_activeZones) {
            zoneNames.push_back(z.name);
        }
        for (NeohubZone& z : this->m_monitoredZones) {
            zoneNames.push_back(z.name);
        }
    }
    loadZoneDataFromNeohub(zoneNames);
}

static void _processZoneCommand(CNeohubManager* _this, const String& command);

// Load the data for the given list of zones (given by name) from the neohub
void CNeohubManager::loadZoneDataFromNeohub(std::vector<String>& zoneNames)
{
    String command;
    int i = 1;
    const int maxZonesPerRequest = 6;  // to avoid responses too long to handle by the
                                       // websocket

    for (String name : zoneNames) {
        if (command == emptyString) command = "{'INFO':['";
        command += name;
        if (name != zoneNames.back() && i < maxZonesPerRequest) {
            command += "','";
            i++;
        }
        else {
            command += "']}";
            _processZoneCommand(this, command);
            command = "";
            i = 1;
        }
    }
}

static void _processZoneCommand(CNeohubManager* _this, const String& command)
{
    String response = _this->neohubCommand(command);
    JsonDocument json;
    DeserializationError error = deserializeJson(json, response);
    if (error) {
        MyLog.printf("Failed to deserialise JSON for INFO: %s\n", error.c_str());
        return;
    }
    if (!json["error"].isNull()) {
        String error = json["error"];
        MyLog.printf("Error retrieving data from Neohub: %s", error.c_str());
        MyLog.printf("Command: %s", command.c_str());
        return;
    }

    JsonArray arr = json["devices"].as<JsonArray>();
    for (JsonVariant obj : arr) {
        String name = obj["device"];
        NeohubZoneData* data = _this->getZoneData(name);
        if (data) {
            data->lastUpdated = time(nullptr);
            data->storeZoneData(obj);
        }
    }
}


// Polling loop - load new zone data every 5 seonds and 
// check the connectionexists every 30 seconds (if no zones are polled in the meantime)
//
// This loop is run in its own task
void CNeohubManager::loop()
{
    const unsigned long pollInterval = 5000;      // How often we poll the zones (ms)
    const unsigned long connectInterval = 30000;  // How often we make sure the connection exists (ms)
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
        this->loadZoneDataFromNeohub(/* all: */ false);
        lastPoll = timeNow;
    }

    // use delay so we don't use too much CPU going round and round in circles...
    delay(100);

    first = false;
}

// The task calling the loop function repeatedly
void CNeohubManager::loopTask(void* parameter)
{
    CNeohubManager* _this = (CNeohubManager*)parameter;
    for (;;) _this->loop();
}

// Mae sure the loop task is running
void CNeohubManager::ensureLoopTask()
{
    if (this->m_loopTaskHandle) return;
    xTaskCreate(
        CNeohubManager::loopTask,  // Task function
        "NeohubLoop",              // Task name
        8096,                      // Stack size (bytes)
        this,                      // Parameter to pass
        1,                         // Task priority
        &this->m_loopTaskHandle    // Task handle
    );
}

CNeohubManager NeohubManager;
