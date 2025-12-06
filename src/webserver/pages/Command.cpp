#include "../HtmlGenerator.h"
#include "../MyWebServer.h"
#include "MyLog.h"

// Ececute commands in the following form:
//
// {
//      "command": "xxxx";
//      "parameters": {
//          ....
//      }
// }
//
// or
//
// {
//      "command": "xxxx";
//      "parameter": ...; }
// }
//
// for single-parameter commands
//
extern OneWireManager oneWireSensors;

static AsyncWebServerResponse* makeCommandResponse(
    AsyncWebServerRequest* request, int responseCode,
    const String& responseString
)
{
    AsyncWebServerResponse* response = request->beginResponse(responseCode, "application/json", responseString);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    return response;
};

void CMyWebServer::executeCommand(AsyncWebServerRequest* request)
{
    JsonDocument commandJson;
    JsonDocument responseJson;
    int responseCode = 200;
    String responseString;

    const String* body = (String*)request->_tempObject;
    if (!body || *body == emptyString) {
        request->send(makeCommandResponse(request, 400, "{ \"error\": \"Empty request\" }"));
        return;
    }

    DeserializationError error = deserializeJson(commandJson, *body);
    if (error) {
        request->send(makeCommandResponse(request, 400, "{ \"error\": \"Invalid Json\" }"));
        return;
    }

    String command = commandJson["command"].as<String>();
    if (command == "null" || command == emptyString) {
        request->send(makeCommandResponse(request, 400, "{ \"error\": \"No commandin Json\" }"));
        return;
    }

    // SetValvePosition (bool automatic, double position)
    if (command == "SetValvePosition") {
        bool automatic = commandJson["parameters"]["automatic"].as<bool>();
        double position = commandJson["parameters"]["position"].as<double>();
        if (automatic) {
            m_valveManager->resumeAutomaticValveControl();
            MyLog.println("Valve returned to automatic control");
        } else {
            m_valveManager->setManualValvePosition(position);
            MyLog.printf("Valve under manual control, position = %.0f\n", position);
        }
    }

    else if (command == "DebumpFlowPid") {
        m_valveManager->setFlowSetpoint(m_valveManager->getFlowSetpoint());
    }

    else if (command == "DebumpValvePid") {
        m_valveManager->setValvePosition(m_valveManager->getValvePosition());
    }

    else if (command == "SensorScan") {
        UBaseType_t prio = uxTaskPriorityGet(NULL);
        vTaskPrioritySet(NULL, 13);

        oneWireSensors.scanForSensors();
        if (oneWireSensors.getCount() == 0) oneWireSensors.scanForSensors();
        
        for (int i = 0; i < oneWireSensors.getCount(); i++) {
            const char* id = oneWireSensors[i].id;
            if (this->m_sensorMap->getNameForId(id).isEmpty()) {
                this->m_sensorMap->setNameForId(id, id);
            }
        }
        oneWireSensors.readAllSensors();

        vTaskPrioritySet(NULL, prio);
        request->send(makeCommandResponse(request, 200, "{ \"reload\": true }"));
        return;
    }

    request->send(makeCommandResponse(request, 200, "{ }"));

}
