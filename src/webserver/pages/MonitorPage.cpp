#include "../MyWebServer.h"

const String &CMyWebServer::mapSensorName(const String &id) const{
  const String &displayName = this->m_sensorMap->getNameForId(id);
  if (displayName.isEmpty()) return id;
  return displayName;
}

void CMyWebServer::respondWithMonitorPage(AsyncWebServerRequest *request) {
  AsyncResponseStream *response = startHttpHtmlResponse(request);
  HtmlGenerator html(response);

  html.navbar(NavbarPage::Monitor);

  html.blockLayout([this, &html]{

    html.block("Control", [this, &html]{
      html.element("table", "class='field-table center-all-td'", [this, &html] {
        html.print("<thead><tr>");
        html.print("<th style='border-bottom: none; min-width: 130px'></th><th style='width: 5em' class='gap-right'>Setpoint</th>");
        html.print("<th style='width: 5em' class='gap-right'>Actual</th><th style='width: 5em' >Diff.</th>");
        html.print("</tr></thead>");

        html.fieldTableRow("Room", [&html]{
          double sp = Config.getRoomSetpoint();
          double t = 20.5;
          double d = t - sp;
          html.element("td", "id='roomSetpoint' class='has-data'",  String(Config.getRoomSetpoint(),1).c_str());
          html.element("td", "id='roomTemperature' class='has-data'", "??.?");
          html.element("td", "id='roomError' class='has-data'", "??.?");
        });
        html.fieldTableRow("Flow", [this, &html]{
          double sp = Config.getFlowSetpoint();
          double t = this->m_valveManager->inputs.flowTemperature;
          double d = t - sp;
          html.element("td", "id='flowSetpoint' class='has-data'", String(sp,1).c_str());
          html.element("td", "id='flowTemperature' class='has-data'", String(t,1).c_str());
          html.element("td", "id='flowError' class='has-data'", String(d,1).c_str());
        });
        html.fieldTableRow("Valve", [this, &html]{
          html.element("td", "id='valvePosition' class='has-data'", (String(this->m_valveManager->outputs.targetValvePosition,0) + "%").c_str());
        });
      });
    });

    html.block("Zones", [this, &html]{
      html.element("table", "class='monitor-table tight'", [this, &html]{
        html.print("<thead>");
        html.print("<tr class='tight'><th rowspan=2 style='vertical-align: bottom' class='gap-right'>Zone</th><th colspan=4>Temperatures</th></tr>");
        html.print("<tr class='tight'><th>Air</th><th>Floor</th></tr>");
        html.print("</thead>");
        html.element("tbody", [this, &html]{
          for (NeohubZone z: NeohubManager.getActiveZones()) {
            NeohubZoneData *d = NeohubManager.getZoneData(z.id);
            if (!d) continue;
            html.element("tr", [this, &html, d] {

              html.element("th", "class='gap-right active'", [d, &html]{
                html.print(d->zone.name.c_str());
              });

              String p = "id=z" + String(d->zone.id) + "-room-temp class='has-data";
              if (!d->demand) p += " off";
              p += "'";

              html.element("td", p.c_str(), [d, &html]{
                if (d->roomTemperature != NeohubZoneData::NO_TEMPERATURE) {
                  html.print(String(d->roomTemperature,1).c_str());
                }
              });

              p = "id=z" + String(d->zone.id) + + "-floor-temp class='has-data";
              if (d->floorLimitTriggered) p += " off";
              p += "'";

              html.element("td", p.c_str(), [d, &html]{
                if (d->floorTemperature != NeohubZoneData::NO_TEMPERATURE) {
                  html.print(String(d->floorTemperature,1).c_str());
                }
              });
            });
          }

          for (NeohubZone z: NeohubManager.getMonitoredZones()) {
            NeohubZoneData *d = NeohubManager.getZoneData(z.id);
            if (!d) continue;
            html.element("tr", [this, &html, d] {

              html.element("th", "class='gap-right'", [d, &html]{
                html.print(d->zone.name.c_str());
              });

              String p = "id=z" + String(d->zone.id) + + "-room-temp class='has-data";
              if (!d->demand) p += " off";
              p += "'";

              html.element("td", p.c_str(), [d, &html]{
                if (d->roomTemperature != NeohubZoneData::NO_TEMPERATURE) {
                  html.print(String(d->roomTemperature,1).c_str());
                }
              });

              p = "id=z" + String(d->zone.id) + + "-floor-temp class='has-data";
              if (d->floorLimitTriggered) p += " off";
              p += "'";

              html.element("td", p.c_str(), [d, &html]{
                if (d->floorTemperature != NeohubZoneData::NO_TEMPERATURE) {
                  html.print(String(d->floorTemperature,1).c_str());
                }
              });
            });
          }
        });
      });
    });

    html.block("Sensors", [this, &html]{
      html.element("table", "class='monitor-table'", [this, &html]{
        html.print("<thead>");
        html.print("<tr class='tight'><th rowspan=2 style='vertical-align: bottom' class='gap-right'>Sensor</th><th rowspan=2 style='vertical-align: bottom' class='gap-right'>Temp</th><th colspan=4>Errors</th></tr>");
        html.print("<tr class='tight'><th>CRC</th><th>Empty</th><th>Other</th><th>Fail</th></tr>");
        html.print("</thead>");
        html.element("tbody", [this, &html]{
          int sensorCount = this->m_sensorMap->getCount();
          int totalReadings = 0;
          int totalCrcErrors = 0;
          int totalNoResponseErrors = 0;
          int totalOtherErrors = 0;
          int totalFailures = 0;
          for (int i = 0; i < sensorCount; i++) {
            SensorMapEntry * entry = (*m_sensorMap)[i];
            Sensor * sensor = m_sensorManager->getSensor(entry->id.c_str());
            float temperature = SensorManager::SENSOR_NOT_FOUND;
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
            if (temperature == SensorManager::SENSOR_NOT_FOUND) html.print("???");
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
            html.print("</tr>");
          }
        });
      });
    });
  }); // block layout

  html.print("<script>setInterval(monitorPage_refreshData, 5000)</script>");
  finishHttpHtmlResponse(response);
  request->send(response);
}

void CMyWebServer::respondWithStatusData(AsyncWebServerRequest *request) {
  JsonDocument statusJson;

  {
    double sp = Config.getRoomSetpoint();
    double t = 20.5;
    statusJson["roomSetpoint"]    = sp;
    if (t != NeohubZoneData::NO_TEMPERATURE) {
      statusJson["roomTemperature"] = t;
      statusJson["roomError"]  = t = sp;
    }
  }
  {
    double sp = Config.getFlowSetpoint();
    double t = this->m_valveManager->inputs.flowTemperature;
    statusJson["flowSetpoint"]    = sp;
    if (t != NeohubZoneData::NO_TEMPERATURE) {
      statusJson["flowTemperature"] = t;
      statusJson["flowError"]  = t - sp;
    }
  }

  statusJson["valvePosition"]   = m_valveManager->outputs.targetValvePosition;
  
  int sensorCount = this->m_sensorMap->getCount();

  for (int i = 0; i < sensorCount; i++) {
      SensorMapEntry * entry = (*this->m_sensorMap)[i];
      Sensor * sensor = m_sensorManager->getSensor(entry->id.c_str());
      if (sensor) {
        statusJson["sensors"][i]["id"] = entry->id;
        statusJson["sensors"][i]["name"] = entry->name;
        statusJson["sensors"][i]["temperature"] = sensor->calibratedTemperature();
        statusJson["sensors"][i]["readings"] = sensor->readings;
        statusJson["sensors"][i]["crcErrors"] = sensor->crcErrors;
        statusJson["sensors"][i]["noResponseErrors"] = sensor->noResponseErrors;
        statusJson["sensors"][i]["otherErrors"] = sensor->otherErrors;
        statusJson["sensors"][i]["failures"] = sensor->failures;
      }
  }
  int i = 0;
  for (NeohubZone z: NeohubManager.getActiveZones()) {
    NeohubZoneData *d = NeohubManager.getZoneData(z.id);
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
      i++;
    }
  }
  for (NeohubZone z: NeohubManager.getMonitoredZones()) {
    NeohubZoneData *d = NeohubManager.getZoneData(z.id);
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
      i++;
    }
  }

  String result;
  serializeJsonPretty(statusJson, result);

  AsyncWebServerResponse* response = request->beginResponse(200, "application/json", result);
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type");
  request->send(response);
}
