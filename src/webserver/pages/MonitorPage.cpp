#include "../MyWebServer.h"

const String &CMyWebServer::mapSensorName(const String &id) const{
  const String &displayName = this->m_sensorMap->getNameForId(id);
  if (displayName.isEmpty()) return id;
  return displayName;
}

size_t CMyWebServer::respondWithMonitorPage(WiFiClient &client) {
  size_t totalBytesSent = 0;

  int sensorCount = this->m_sensorMap->getCount();

  totalBytesSent += this->startHttpHtmlResponse(client, "200 OK", 5);
  totalBytesSent += client.println("<table><tbody>");
  totalBytesSent += client.println("<tr><th colspan=99>Boiler Control</th>");

  totalBytesSent += client.print("<tr>");
  totalBytesSent += client.print("<th>Flow Setpoint</th><td>");
      client.print(Config.getFlowTargetTemp(),1);
  totalBytesSent += client.print("</td><td colspan=3></td></tr>");

  totalBytesSent += client.print("<tr>");
  totalBytesSent += client.print("<th>Valve Position</th><td>");
      client.print(m_valveManager->outputs.targetValvePosition,1);
  totalBytesSent += client.print("%</td><td colspan=4></td></tr>");

  int totalReadings = 0;
  int totalCrcErrors = 0;
  int totalNoResponseErrors = 0;
  int totalFailures = 0;

  totalBytesSent += client.println("<tr><th rowspan=2>Sensor</th><th rowspan=2>Temp</th><th colspan=3>Errors</th></tr>");
  totalBytesSent += client.println("<tr><th>CRC</th><th>Empty</th><th>Fail</th></tr>");
  for (int i = 0; i < sensorCount; i++) {
    SensorMapEntry * entry = (*m_sensorMap)[i];
    Sensor * sensor = m_sensorManager->getSensor(entry->id.c_str());
    float temperature = SensorManager::SENSOR_NOT_FOUND;
    float calibatedTemperature = SensorManager::SENSOR_NOT_FOUND;
    if (sensor) {
      temperature = sensor->temperature;
      calibatedTemperature = sensor->calibratedTemperature();
      totalReadings += sensor->readings;
      totalCrcErrors += sensor->crcErrors;
      totalNoResponseErrors += sensor->noResponseErrors;
      totalFailures += sensor->failures;
    }

    totalBytesSent += client.println("<tr>");
    totalBytesSent += client.print("<th>");
    totalBytesSent += client.print(entry->name);
    totalBytesSent += client.println("</th>");
    totalBytesSent += client.print("<td>");
    if (calibatedTemperature == SensorManager::SENSOR_NOT_FOUND) totalBytesSent += client.print("???");
    if (calibatedTemperature > -50) totalBytesSent += client.print(calibatedTemperature, 1);
    totalBytesSent += client.println("</td>");
    if (sensor) {
      totalBytesSent += client.printf("<td>%d</td><td>%d</td><td>%d</td>", sensor->crcErrors, sensor->noResponseErrors, sensor->failures);
    }
    else {
      totalBytesSent += client.print("<td colspan=3></td>");
    }

    totalBytesSent += client.println("</tr>");
  }

  totalBytesSent += client.println("</body><table>");
  totalBytesSent += finishHttpHtmlResponse(client);
  return totalBytesSent;
}

