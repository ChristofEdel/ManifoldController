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
      html.fieldTable( [this, &html] {
        html.fieldTableRow("Flow Setpoint", [&html]{
          html.element("td", "id='flowSetpoint' class='has-data'", String(Config.getFlowTargetTemp(),1).c_str());
        });
        html.fieldTableRow("Valve Position", [this, &html]{
          html.element("td", "id='valvePosition' class='has-data'", (String(this->m_valveManager->outputs.targetValvePosition,0) + "%").c_str());
        });
      });
    });

    html.block("Sensors", [this, &html]{
      html.element("table", "class='monitor-table'", [this, &html]{
        html.print("<thead>");
        html.print("<tr class='tight'><th rowspan=2 style='vertical-align: bottom' class='gap-right'>Sensor</th><th rowspan=2 style='vertical-align: bottom' class='gap-right'>Temp</th><th colspan=4>Errors</th></tr>");
        html.print("<tr class='tight'><th>CRC</th><th>Empty</th><th>Other</th><th>Fail</th></tr>");
        html.print("</thead>");
        html.element("tbody", "class='sensorList'", [this, &html]{
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

