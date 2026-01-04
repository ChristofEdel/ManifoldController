#include "../MyWebServer.h"
#include "NeohubZoneManager.h"
#include "ValveManager.h"
#include "StringTools.h"
#include "WeatherDataManager.h"

const String &CMyWebServer::mapSensorName(const String &id) const{
  const String &displayName = SensorMap.getNameForId(id);
  if (displayName.isEmpty()) return id;
  return displayName;
}

void CMyWebServer::respondWithMonitorPage(AsyncWebServerRequest *request) {
  bool expertMode = true;
  AsyncResponseStream *response = startHttpHtmlResponse(request);
  HtmlGenerator html(response);

  html.navbar(NavbarPage::Monitor);

  html.blockLayout([this, expertMode, &html]{

    html.block("Control", [this, expertMode, &html]{
      html.element("table", "class='field-table center-all-td'", [this, expertMode, &html] {
        html.print("<thead><tr>");
        html.print("<th style='border-bottom: none; min-width: 130px'></th><th style='width: 5em' class='gap-right'>Setpoint</th>");
        html.print("<th style='width: 5em' class='gap-right'>Actual</th><th style='width: 5em' class='gap-right' >&Delta;t</th>");
        if (expertMode) {
          html.print("<th style='width: 5em' class='gap-right'>P</th><th style='width: 5em' >I</th>");
        }
        html.print("</tr></thead>");

        time_t now = time(nullptr);

        html.fieldTableRow("Room", [this, now, expertMode, &html]{
          double sp = Config.getRoomSetpoint();
          double t = ValveManager.inputs.roomTemperature;
          double d = t - sp;
          bool aged = ValveManager.timestamps.isAged(now, ValveManager.timestamps.roomDataLoadTime);
          bool dead = ValveManager.timestamps.isDead(now, ValveManager.timestamps.roomDataLoadTime);
          const char * extraClass = dead ? " data-is-dead" : aged ? " data-is-aged" : "";
          html.element("td", "id='roomSetpoint' class='has-data'",  String(sp,1).c_str());
          html.element("td", StringPrintf("id='roomTemperature' class='has-data%s'", extraClass).c_str(), t > -50 ? String(t,1).c_str() : "");
          html.element("td", "id='roomError' class='has-data'", t > -50 ? String(d,1).c_str() : "");
          if (expertMode) {
            html.element("td", "id='roomP' class='has-data'", String(ValveManager.getRoomProportionalTerm(),1).c_str());
            html.element("td", "id='roomI' class='has-data'", String(ValveManager.getRoomIntegralTerm(),1).c_str());
          }
          html.print(StringPrintf("<td id='roomAged' class='data-is-aged'%s>OLD</td>", ValveManager.timestamps.isAged(now, ValveManager.timestamps.flowCalculatedTime) ? "" : "style='display: none'").c_str());
          html.print(StringPrintf("<td id='roomDead' class='data-is-dead'%s>DEAD</td>", ValveManager.timestamps.isDead(now, ValveManager.timestamps.flowCalculatedTime) ? "" : "style='display: none'").c_str());
        });
        if (NeohubZoneManager.getActiveZones().size() == 1 && NeohubZoneManager.getMonitoredZones().size() == 0) {
          NeohubZoneData *d = NeohubZoneManager.getZoneData(NeohubZoneManager.getActiveZones().back().id);
          if (d && d->floorTemperature != NeohubZoneData::NO_TEMPERATURE) {
            html.fieldTableRow("Floor", [d, &html]{
              html.print("<td></td>");
              double ft = d->floorTemperature;
              String p = "id=z" + String(d->zone.id) + "-floor-temp class='has-data";
              if (d->floorLimitTriggered) p += " off";
              p += "'";
              html.element("td", p.c_str(),  String(ft,1).c_str());
            });
          }
        }
        html.fieldTableRow("Flow", [this, now, expertMode, &html]{
          double sp = ValveManager.getFlowSetpoint();
          double t = ValveManager.inputs.flowTemperature;
          double d = t - sp;
          bool aged = ValveManager.timestamps.isAged(now, ValveManager.timestamps.flowDataLoadTime);
          bool dead = ValveManager.timestamps.isDead(now, ValveManager.timestamps.flowDataLoadTime);
          const char * extraClass = dead ? " data-is-dead" : aged ? " data-is-aged" : "";
          html.element("td", "id='flowSetpoint' class='has-data' onclick=\"openSetValueDialog(this, 'Set Flow Temperature', 'SetFlowPidOutput')\"", String(sp,1).c_str());
          html.element("td",  StringPrintf("id='flowTemperature' class='has-data%s'", extraClass).c_str(), t > -50 ? String(t,1).c_str() : "");
          html.element("td", "id='flowError' class='has-data'", t > -50 ? String(d,1).c_str() : "");
          if (expertMode) {
            html.element("td", "id='flowP' class='has-data'", String(ValveManager.getFlowProportionalTerm(),1).c_str());
            html.element("td", "id='flowI' class='has-data'", String(ValveManager.getFlowIntegralTerm(),1).c_str());
          }
          html.print(StringPrintf("<td id='flowAged' class='data-is-aged'%s>OLD</td>", ValveManager.timestamps.isAged(now, ValveManager.timestamps.valveCalculatedTime) ? "" : "style='display: none'").c_str());
          html.print(StringPrintf("<td id='flowDead' class='data-is-dead'%s>DEAD</td>", ValveManager.timestamps.isDead(now, ValveManager.timestamps.valveCalculatedTime) ? "" : "style='display: none'").c_str());
        });
        html.fieldTableRow("Valve", [this, &html]{
          html.element("td", "id='valvePosition' class='has-data' onclick=\"openSetValueDialog(this, 'Set Valve Position', 'SetValvePidOutput')\"", (String(ValveManager.getValvePosition(),0) + "%").c_str());
          if (ValveManager.valveUnderManualControl()) {
            html.print("<td id='valveManualFlag' colspan=2 class='manual-control'>Manual Control</td>");
          }
          else {
            html.print("<td id='valveManualFlag' colspan=2 class='manual-control' style='display: none'>Manual Control</td>");
          }
        });
      });
    });

    if (NeohubZoneManager.getActiveZones().size() > 1 || NeohubZoneManager.getMonitoredZones().size() > 0) {
      html.block("Zones", [this, &html]{
        html.element("table", "class='monitor-table tight'", [this, &html]{
          html.print("<thead>");
          html.print("<tr class='tight'><th rowspan=2 style='vertical-align: bottom' class='gap-right'>Zone</th><th colspan=2>Temperatures</th></tr>");
          html.print("<tr class='tight'><th>Air</th><th>Floor</th></tr>");
          html.print("</thead>");
          time_t now = time(nullptr);
          html.element("tbody", [this, now, &html]{
            for (NeohubZone z: NeohubZoneManager.getActiveZones()) {
              NeohubZoneData *d = NeohubZoneManager.getZoneData(z.id);
              if (!d) continue;
              html.element("tr", [this, now, &html, d] {

                html.element("th", "class='gap-right active'", [d, &html]{
                  html.print(d->zone.name.c_str());
                });

                String p = "id=z" + String(d->zone.id) + "-room-temp class='has-data'";
                html.element("td", p.c_str(), [d, &html]{
                  if (d->roomTemperature != NeohubZoneData::NO_TEMPERATURE) {
                    html.print(String(d->roomTemperature,1).c_str());
                  }
                });

                p = "id=z" + String(d->zone.id) + "-floor-temp class='has-data'";
                html.element("td", p.c_str(), [d, &html]{
                  if (d->floorTemperature != NeohubZoneData::NO_TEMPERATURE) {
                    html.print(String(d->floorTemperature,1).c_str());
                  }
                });
                html.print(StringPrintf("<td id='z%d-room-off' class='data-is-off'%s>OFF</td>",  d->zone.id, !d->demand ? "" : " style='display: none'").c_str());
                html.print(StringPrintf("<td id='z%d-floor-off' class='data-is-off'%s>FLOOR</td>", d->zone.id, d->floorLimitTriggered ? "" : " style='display: none'").c_str());
                html.print(StringPrintf("<td id='z%d-aged' class='data-is-aged'%s>OLD</td>", d->zone.id, d->isAged(now) ? "" : " style='display: none'").c_str());
                html.print(StringPrintf("<td id='z%d-dead' class='data-is-dead'%s>DEAD</td>", d->zone.id, d->isDead(now) ? "" : " style='display: none'").c_str());
              });
            }

            for (NeohubZone z: NeohubZoneManager.getMonitoredZones()) {
              NeohubZoneData *d = NeohubZoneManager.getZoneData(z.id);
              if (!d) continue;
              html.element("tr", [this, now, &html, d] {

                html.element("th", "class='gap-right'", [d, &html]{
                  html.print(d->zone.name.c_str());
                });

                String p = "id=z" + String(d->zone.id) + "-room-temp class='has-data'";
                html.element("td", p.c_str(), [d, &html]{
                  if (d->roomTemperature != NeohubZoneData::NO_TEMPERATURE) {
                    html.print(String(d->roomTemperature,1).c_str());
                  }
                });

                p = "id=z" + String(d->zone.id) + "-floor-temp class='has-data'";
                html.element("td", p.c_str(), [d, &html]{
                  if (d->floorTemperature != NeohubZoneData::NO_TEMPERATURE) {
                    html.print(String(d->floorTemperature,1).c_str());
                  }
                });
                html.print(StringPrintf("<td id='z%d-room-off' class='data-is-off'%s>OFF</td>",  d->zone.id, !d->demand ? "" : " style='display: none'").c_str());
                html.print(StringPrintf("<td id='z%d-floor-off' class='data-is-off'%s>FLOOR</td>", d->zone.id, d->floorLimitTriggered ? "" : " style='display: none'").c_str());
                html.print(StringPrintf("<td id='z%d-aged' class='data-is-aged'%s>OLD</td>", d->zone.id, d->isAged(now) ? "" : "style='display: none'").c_str());
                html.print(StringPrintf("<td id='z%d-dead' class='data-is-dead'%s>DEAD</td>", d->zone.id, d->isDead(now) ? "" : "style='display: none'").c_str());
              });
            }
          });
        });
      });
    }

    html.block("Sensors", [this, &html]{
      html.element("table", "class='monitor-table'", [this, &html]{
        html.print("<thead>");
        html.print("<tr class='tight'><th rowspan=2 style='vertical-align: bottom' class='gap-right'>Sensor</th><th rowspan=2 style='vertical-align: bottom' class='gap-right'>Temp</th><th colspan=4>Errors</th></tr>");
        html.print("<tr class='tight'><th>CRC</th><th>Empty</th><th>Other</th><th>Fail</th></tr>");
        html.print("</thead>");
        html.element("tbody", [this, &html]{
          time_t now = time(nullptr);

          html.print("<tr><th>Outside</th>");
          html.print("<td id='outside-temp' class='has-data'>");
          WeatherData& wd = WeatherDataManager.getWeatherData();
          float oat = wd.outsideTemperature;
          if (oat <= -50) html.print("???");
          else html.print(String(oat, 1).c_str());
          html.print("</td>");
          html.print("<td></td><td></td><td></td><td></td>");
          html.print(StringPrintf("<td id='outside-aged' class='data-is-aged'%s>OLD</td>", wd.isAged(now) ? "" : "style='display: none'").c_str());
          html.print(StringPrintf("<td id='outside-dead' class='data-is-dead'%s>DEAD</td>", wd.isDead(now) ? "" : "style='display: none'").c_str());
          html.print("<td></td><td></td><td></td><td></td></tr>");

          int sensorCount = SensorMap.getCount();
          int totalReadings = 0;
          int totalCrcErrors = 0;
          int totalNoResponseErrors = 0;
          int totalOtherErrors = 0;
          int totalFailures = 0;
          for (int i = 0; i < sensorCount; i++) {
            SensorMapEntry * entry = SensorMap[i];
            OneWireSensor * sensor = OneWireManager.getSensor(entry->id.c_str());
            float temperature = COneWireManager::SENSOR_NOT_FOUND;
            if (sensor) {
              temperature = sensor->calibratedTemperature();
              totalReadings += sensor->readings;
              totalCrcErrors += sensor->crcErrors;
              totalNoResponseErrors += sensor->noResponseErrors;
              totalOtherErrors += sensor->otherErrors;
              totalFailures += sensor->failures;
            }

            html.print("<tr>");
            html.print("<th>");
            html.print(entry->name.c_str());
            html.print("</th>");
            html.print("<td id='"); html.print(entry->id.c_str()); html.print("-temp' class='has-data'>");
            if (temperature == COneWireManager::SENSOR_NOT_FOUND) html.print("???");
            if (temperature > -50) html.print(String(temperature, 1).c_str());
            html.print("</td>");
            if (sensor) {
              char buffer[500];
              snprintf(
                buffer, sizeof(buffer), 
                "<td id='%s-crc' class='has-data'>%d</td><td id='%s-nr' class='has-data'>%d</td><td id='%s-oe' class='has-data'>%d</td><td id='%s-f' class='has-data'>%d</td>", 
                sensor->id, sensor->crcErrors, 
                sensor->id, sensor->noResponseErrors, 
                sensor->id, sensor->otherErrors,
                sensor->id, sensor->failures
              );
              html.print(buffer);
            }
            else {
              html.print("<td colspan=99></td>");
            }
            html.print(StringPrintf("<td id='%s-aged' class='data-is-aged'%s>OLD</td>", sensor->id, sensor->isAged(now) ? "" : "style='display: none'").c_str());
            html.print(StringPrintf("<td id='%s-dead' class='data-is-dead'%s>DEAD</td>", sensor->id, sensor->isDead(now) ? "" : "style='display: none'").c_str());
            html.print("</tr>");
          }
        });
      });
    });
  }); // block layout
  html.print("<div id='changeDialog' title='Set XXX' style='display:none;'><input id='newValue' type='text'></div>");
  html.footer();


  html.print("<script>setInterval(monitorPage_refreshData, 5000)</script>");
  finishHttpHtmlResponse(response);
  request->send(response);
}

void CMyWebServer::respondWithStatusData(AsyncWebServerRequest *request) {
  JsonDocument statusJson;
  time_t now = time(nullptr);

  {
    double sp = Config.getRoomSetpoint();
    double t = ValveManager.inputs.roomTemperature;
    
    statusJson["roomSetpoint"]    = sp;
    statusJson["roomTemperature"] = t;
    statusJson["roomError"]  = t - sp;
    if (t >= NeohubZoneData::NO_TEMPERATURE) {
      statusJson["roomTemperatureAged"] = ValveManager.timestamps.isAged(now, ValveManager.timestamps.roomDataLoadTime);
      statusJson["roomTemperatureDead"] = ValveManager.timestamps.isDead(now, ValveManager.timestamps.roomDataLoadTime);
    }
    statusJson["roomProportionalTerm"] = ValveManager.getRoomProportionalTerm();
    statusJson["roomIntegralTerm"] = ValveManager.getRoomIntegralTerm();
    statusJson["roomAged"] = ValveManager.timestamps.isAged(now, ValveManager.timestamps.flowCalculatedTime);
    statusJson["roomDead"] = ValveManager.timestamps.isDead(now, ValveManager.timestamps.flowCalculatedTime);
  }
  {
    double sp = ValveManager.getFlowSetpoint();
    double t = ValveManager.inputs.flowTemperature;
    statusJson["flowSetpoint"] = sp;
    statusJson["flowTemperature"] = t;
    statusJson["flowError"]       = t - sp;
    if (t >= NeohubZoneData::NO_TEMPERATURE) {
      statusJson["flowTemperatureAged"] = ValveManager.timestamps.isAged(now, ValveManager.timestamps.flowDataLoadTime);
      statusJson["flowTemperatureDead"] = ValveManager.timestamps.isDead(now, ValveManager.timestamps.flowDataLoadTime);
    }
    statusJson["flowProportionalTerm"] = ValveManager.getFlowProportionalTerm();
    statusJson["flowIntegralTerm"]     = ValveManager.getFlowIntegralTerm();
    statusJson["flowAged"] = ValveManager.timestamps.isAged(now, ValveManager.timestamps.valveCalculatedTime);
    statusJson["flowDead"] = ValveManager.timestamps.isDead(now, ValveManager.timestamps.valveCalculatedTime);
  }

  statusJson["valvePosition"]        = ValveManager.getValvePosition();
  statusJson["valveManualControl"]   = ValveManager.valveUnderManualControl();
  statusJson["uptimeText"]           = uptimeText();
  
  int sensorCount = SensorMap.getCount();

  int i = 0;
  for (int sensorIndex = 0; sensorIndex < sensorCount; sensorIndex++) {
    SensorMapEntry * entry = SensorMap[sensorIndex];
    OneWireSensor * sensor = OneWireManager.getSensor(entry->id.c_str());
    if (sensor) {
      statusJson["sensors"][i]["id"] = entry->id;
      statusJson["sensors"][i]["name"] = entry->name;
      statusJson["sensors"][i]["temperature"] = sensor->calibratedTemperature();
      statusJson["sensors"][i]["readings"] = sensor->readings;
      statusJson["sensors"][i]["crcErrors"] = sensor->crcErrors;
      statusJson["sensors"][i]["noResponseErrors"] = sensor->noResponseErrors;
      statusJson["sensors"][i]["otherErrors"] = sensor->otherErrors;
      statusJson["sensors"][i]["failures"] = sensor->failures;
      statusJson["sensors"][i]["isAged"] = sensor->isAged(now);
      statusJson["sensors"][i]["isDead"] = sensor->isDead(now);
      i++;
    }
  }
  WeatherData& wd = WeatherDataManager.getWeatherData();
  statusJson["sensors"][i]["id"] = "outside";
  statusJson["sensors"][i]["name"] = "Outside";
  statusJson["sensors"][i]["temperature"] = wd.outsideTemperature;
  statusJson["sensors"][i]["isAged"] = wd.isAged(now);
  statusJson["sensors"][i]["isDead"] = wd.isDead(now);

  i = 0;
  for (NeohubZone z: NeohubZoneManager.getActiveZones()) {
    NeohubZoneData *d = NeohubZoneManager.getZoneData(z.id);
    if (d) {
      statusJson["zones"][i]["id"] = d->zone.id;
      if (d->roomTemperature != NeohubZoneData::NO_TEMPERATURE) {
        statusJson["zones"][i]["roomTemperature"] = d->roomTemperature;
      }
      if (d->floorTemperature != NeohubZoneData::NO_TEMPERATURE) {
        statusJson["zones"][i]["foorTemperature"] = d->floorTemperature;
      }
      statusJson["zones"][i]["roomOff"] = !d->demand;
      statusJson["zones"][i]["floorOff"] = d->floorLimitTriggered;
      statusJson["zones"][i]["isAged"] = d->isAged(now);
      statusJson["zones"][i]["isDead"] = d->isDead(now);
      i++;
    }
  }
  for (NeohubZone z: NeohubZoneManager.getMonitoredZones()) {
    NeohubZoneData *d = NeohubZoneManager.getZoneData(z.id);
    if (d) {
      statusJson["zones"][i]["id"] = d->zone.id;
      if (d->roomTemperature != NeohubZoneData::NO_TEMPERATURE) {
        statusJson["zones"][i]["roomTemperature"] = d->roomTemperature;
      }
      if (d->floorTemperature != NeohubZoneData::NO_TEMPERATURE) {
        statusJson["zones"][i]["foorTemperature"] = d->floorTemperature;
      }
      statusJson["zones"][i]["roomOff"] = !d->demand;
      statusJson["zones"][i]["floorOff"] = d->floorLimitTriggered;
      statusJson["zones"][i]["isAged"] = d->isAged(now);
      statusJson["zones"][i]["isDead"] = d->isDead(now);
      i++;
    }
  }

  // Debug code - delay the response so we can test dropped TCP connections
  // static int countdown = 3;
  // if (countdown == 0) {
  //   MyDebugLog.printf("******************* |    delayed response\n");
  //   delay(20000);
  // }
  // countdown--;

  String result;
  serializeJsonPretty(statusJson, result);

  AsyncWebServerResponse* response = request->beginResponse(200, "application/json", result);
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type");
  request->send(response);
}
