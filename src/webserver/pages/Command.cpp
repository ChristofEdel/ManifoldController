#include "HtmlGenerator.h"
#include "../MyWebServer.h"
#include "MyLog.h"
#include "NeohubManager.h"
#include "ValveManager.h"
#include "StringTools.h"

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

void CMyWebServer::respondToOptionsRequest(AsyncWebServerRequest* request) {
    AsyncWebServerResponse* response = request->beginResponse(200, "application/json");
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    request->send(response);
}


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

    else if (command == "SetFlowPidOutput") {
        if (commandJson["value"].isNull()) {
            ValveManager.setFlowSetpoint(ValveManager.getFlowSetpoint());
        }
        else {
            double value = commandJson["value"].as<double>();
            if (value > 0) {
                ValveManager.setFlowSetpoint(value);
            }
        }
        // default response
    }

    else if (command == "SetValvePidOutput") {
        if (commandJson["value"].isNull()) {
            ValveManager.setValvePosition(ValveManager.getFlowSetpoint());
        }
        else {
            double value = commandJson["value"].as<double>();
            if (value > 0) {
                ValveManager.setValvePosition(value);
            }
        }
        // default response
    }

    else if (command == "GetZoneStatus" || command == "ZoneOn" || command == "ZoneOff" || command == "ZoneAuto") {
        int zoneId = commandJson["zoneId"].as<int>();
        String zoneName = NeohubManager.getZoneName(zoneId);
        if (zoneName == emptyString) {
            request->send(makeCommandResponse(request, 400, "{ \"error\": \"unknown zone\" }"));
            return;
        }
        NeohubZoneData* zd;
        if (command == "GetZoneStatus") zd = NeohubManager.getZoneData(zoneName, /* forceLoad: */ true);
        else if (command == "ZoneOn") zd = NeohubManager.forceZoneOn(zoneName);
        else if (command == "ZoneOff") zd = NeohubManager.forceZoneOff(zoneName);
        else if (command == "ZoneAuto") zd = NeohubManager.setZoneToAutomatic(zoneName);
        if (!zd) {
            request->send(makeCommandResponse(request, 400, "{ \"error\": \"unable to configure zone\" }"));
            return;
        }
        request->send(makeCommandResponse(request, 200, 
            StringPrintf(
                R"({ "setpoint": %.1f, "on": %s})",
                zd->roomSetpoint, BOOL_TO_STRING(zd->demand)
            )
        ));
        return;
    }

    else if (command == "SensorScan") {
        UBaseType_t prio = uxTaskPriorityGet(NULL);
        vTaskPrioritySet(NULL, 13);

        MyLog.println("Scanning for OneWire sensors");
        OneWireManager.scanForSensors();
        if (OneWireManager.getCount() == 0) OneWireManager.scanForSensors();
        MyLog.printf("%d sensors present\n", OneWireManager.getCount());
        
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
