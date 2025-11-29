#include "NeohubConnection.h"
#include <vector>
#include <ArduinoJson.h>


// A NeohubManager is a global singleton that retrieves and maintains data
// for a neohub using an underlying NeohibConnection.

struct NeohubThermostatData {

    int id;
    String name;

    time_t lastUpdated = 0;
    double temperature = NO_TEMPERATURE;
    double temperatureSetpoint = NO_TEMPERATURE;
    double floorTemperature = NO_TEMPERATURE;
    double floorTemperatureLimit = NO_TEMPERATURE;
    bool floorLimitTriggered = false;
    bool online = false;

    bool pollInLoop = false;

    static const int NO_TEMPERATURE = -50;
private:
    bool found = true;
    friend class CNeohubManager;

};

class CNeohubManager {

    public:

        CNeohubManager() {
            m_thermostatData.reserve(15);
        }

        const std::vector<NeohubThermostatData>& getThermostatData() { ensureThermostatNames(); return m_thermostatData; }

        // Get the data for a particular thermostat. If it has not been polled,
        // it will be empty except for its name and id
        NeohubThermostatData * getThermostatData(const String &name);
        NeohubThermostatData * getThermostatData(int id);

        // Lead data for sensors from the neohub. Establish the connection
        // if necessary
        bool loadFromNeohub(NeohubThermostatData *data);
        void loadAllFromNeohub(bool flaggedAsPollInLoopOnly = false);

        // Direct connection to the neohub
        bool ensureNeohubConnection();
        String neohubCommand(const String & command, int timeoutMillis = commandTimeoutMillis);

        static const int connectTimeoutMillis = 5000;
        static const int commandTimeoutMillis = 2000;

    private:
        // Connecetion to Neohub
        String m_url = "192.168.1.139";
        String m_token = "6b4f25a5-9de5-460a-a3ac-5845d3fbe095";
        NeohubConnection *m_connection;


        // Vector with the data for all thermostats
        std::vector<NeohubThermostatData> m_thermostatData;
        NeohubThermostatData *getOrCreateThermostatData(int id, const String &name);
        void ensureThermostatNames();
        void loadThermostatNames();

        // loop function which regularly ensures the connection and polls data as requied
        void loop();

        // The loop task which regularly executes the loop function 
        TaskHandle_t m_loopTaskHandle = nullptr;
        void ensureLoopTask();
        static void loopTask(void *parameter);

};

extern CNeohubManager NeohubManager;