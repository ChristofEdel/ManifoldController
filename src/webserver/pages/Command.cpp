#include "../HtmlGenerator.h"
#include "../MyWebServer.h"
#include "MyLog.h"
#include "NeohubManager.h"
#include "ValveManager.h"

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
        request->send(makeCommandResponse(request, 400, "{ \"error\": \"No command in Json\" }"));
        return;
    }

    // SetValvePosition (bool automatic, double position)
    if (command == "SetValvePosition") {
        bool automatic = commandJson["parameters"]["automatic"].as<bool>();
        double position = commandJson["parameters"]["position"].as<double>();
        if (automatic) {
            ValveManager.resumeAutomaticValveControl();
            MyLog.println("Valve returned to automatic control");
        } else {
            ValveManager.setManualValvePosition(position);
            MyLog.printf("Valve under manual control, position = %.0f\n", position);
        }
        // default response
    }

    else if (command == "DebumpFlowPid") {
        ValveManager.setFlowSetpoint(ValveManager.getFlowSetpoint());
        // default response
    }

    else if (command == "DebumpValvePid") {
        ValveManager.setValvePosition(ValveManager.getValvePosition());
        // default response
    }

    // else if (command == "ZoneOn") {
    //     int zoneId = commandJson["zone"].as<int>();
    //     NeohubManager.forceZoneOn(zoneId);
    //    // default response
    // }

    // else if (command == "ZoneOff") {
    //     int zoneId = commandJson["zone"].as<int>();
    //     NeohubManager.forceZoneOff(zoneId);
    //    // default response
    // }

    // else if (command == "ZoneAuto") {
    //     int zoneId = commandJson["zone"].as<int>();
    //     float setpoint = commandJson["setpoint"].as<int>();
    //     NeohubManager.setZoneToAutomatic(setpoint);
    //    // default response
    // }

    else if (command == "ZoneAuto") {
        ValveManager.setValvePosition(ValveManager.getValvePosition());
        // default response
    }

    else if (command == "SensorScan") {
        UBaseType_t prio = uxTaskPriorityGet(NULL);
        vTaskPrioritySet(NULL, 13);

        OneWireManager.scanForSensors();
        if (OneWireManager.getCount() == 0) OneWireManager.scanForSensors();
        
        for (int i = 0; i < OneWireManager.getCount(); i++) {
            const char* id = OneWireManager[i].id;
            if (
                SensorMap.getNameForId(id).isEmpty()) {
                SensorMap.setNameForId(id, id);
            }
        }
        OneWireManager.readAllSensors();

        vTaskPrioritySet(NULL, prio);
        request->send(makeCommandResponse(request, 200, "{ \"reload\": true }"));
        return;
    }
    else {
        request->send(makeCommandResponse(request, 400, "{ \"error\": \"Unknown Command\" }"));
    }

    // default response so we don't have to repeat this for every
    // command with an empty response
    request->send(makeCommandResponse(request, 200, "{ }"));

}
