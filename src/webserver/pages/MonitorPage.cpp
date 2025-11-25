#include "../MyWebServer.h"

const String &CMyWebServer::mapSensorName(const String &id) const{
  const String &displayName = this->m_sensorMap->getNameForId(id);
  if (displayName.isEmpty()) return id;
  return displayName;
}

void CMyWebServer::respondWithMonitorPage(AsyncWebServerRequest *request) {
  AsyncResponseStream *response = startHttpHtmlResponse(request);
  int sensorCount = this->m_sensorMap->getCount();
  this->startHttpHtmlResponse(request);
  response->println("<table><tbody>");
  response->println("<tr><th colspan=99>Boiler Control</th>");

  response->print("<tr>");
  response->print("<th>Flow Setpoint</th><td id='flowSetpoint' class='has-data'>");
      response->print(Config.getFlowTargetTemp(),1);
  response->print("</td><td colspan=4></td></tr>");

  response->print("<tr>");
  response->print("<th>Valve Position</th><td id='valvePosition' class='has-data'>");
      response->print(m_valveManager->outputs.targetValvePosition,1);
  response->print("%</td><td colspan=4></td></tr>");

  int totalReadings = 0;
  int totalCrcErrors = 0;
  int totalNoResponseErrors = 0;
  int totalOtherErrors = 0;
  int totalFailures = 0;

  response->println("<tr><th rowspan=2>Sensor</th><th rowspan=2>Temp</th><th colspan=4>Errors</th></tr>");
  response->println("<tr><th>CRC</th><th>Empty</th><th>Other</th><th>Fail</th></tr>");
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

    response->println("<tr>");
    response->print("<th>");
    response->print(entry->name);
    response->println("</th>");
    response->printf("<td id='%s-temp' class='has-data'>", entry->id.c_str());
    if (temperature == SensorManager::SENSOR_NOT_FOUND) response->print("???");
    if (temperature > -50) response->print(temperature, 1);
    response->println("</td>");
    if (sensor) {
      response->printf("<td id='%s-crc' class='has-data'>%d</td><td id='%s-nr' class='has-data'>%d</td><td id='%s-oe' class='has-data'>%d</td><td id='%s-f' class='has-data'>%d</td>", 
        sensor->id, sensor->crcErrors, 
        sensor->id, sensor->noResponseErrors, 
        sensor->id, sensor->otherErrors,
        sensor->id, sensor->failures
      );
    }
    else {
      response->print("<td colspan=3></td>");
    }
    response->println("</tr>");
  }
  response->println("</table>");

  response->println("<script>setInterval(monitorPage_refreshData, 5000)</script>");
  response->println("</body><table>");
  response->println("</body><table>");
  finishHttpHtmlResponse(response);
  request->send(response);
}

