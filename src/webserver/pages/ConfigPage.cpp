#include "../MyWebServer.h"
#include <URLCode.h>

size_t CMyWebServer::respondWithConfigPage(WiFiClient &client) {
  size_t totalBytesSent = 0;

  totalBytesSent += this->startHttpHtmlResponse(client, "200 OK");
  totalBytesSent += client.println("<form action='config' method='post'>");
  
  totalBytesSent += client.println("<table><tbody>");

  totalBytesSent += client.print("<tr><th colspan=99>Heating</th></tr>");

  totalBytesSent += client.print("<tr>");
    totalBytesSent += client.print("<th>Flow Setpoint</th>");
    totalBytesSent += client.print("<td colspan=2><input name='tft' type='text' value='");
    totalBytesSent += client.print(Config.getFlowTargetTemp(),1);
    totalBytesSent += client.print("'/></td>");
  totalBytesSent += client.print("</tr>");

  totalBytesSent += client.println("<tr><th colspan=99>PID Controller</th></tr>");

  totalBytesSent += client.print("<tr>");
    totalBytesSent += client.print("<th>Proportional Gain</th>");
    totalBytesSent += client.print("<td colspan=2><input name='pg' type='text' value='");
    totalBytesSent += client.print(Config.getProportionalGain(),1);
    totalBytesSent += client.print("'/></td>");
  totalBytesSent += client.print("</tr>");

  totalBytesSent += client.print("<tr>");
    totalBytesSent += client.print("<th>Integral Seconds</th>");
    totalBytesSent += client.print("<td colspan=2><input name='is' type='text' value='");
    totalBytesSent += client.print(Config.getIntegralSeconds(),1);
    totalBytesSent += client.print("'/></td>");
  totalBytesSent += client.print("</tr>");

  totalBytesSent += client.print("<tr>");
    totalBytesSent += client.print("<th>Derivative Seconds</th>");
    totalBytesSent += client.print("<td colspan=2><input name='ds' type='text' value='");
    totalBytesSent += client.print(Config.getDerivativeSeconds(),1);
    totalBytesSent += client.print("'/></td>");
  totalBytesSent += client.print("</tr>");


  totalBytesSent += client.println("<tr><th colspan=99>Sensor Config</th></tr>");

  totalBytesSent += client.print("<tr>");
    totalBytesSent += client.print("<th>Sensor ID</th>");
    totalBytesSent += client.print("<th colspan=2>Name</th>");
  totalBytesSent += client.println("</tr>");
  totalBytesSent += client.println("</tbody><tbody class='sensorList'>");

  int sensorCount = this->m_sensorMap->getCount();
  for (int i = 0; i < sensorCount; i++) {
    SensorMapEntry * entry = (*m_sensorMap)[i];
    totalBytesSent += client.print("<tr>");
      totalBytesSent += client.print("<th class='handle'>");
        totalBytesSent += client.print(entry->id);
      totalBytesSent += client.print("</th>");
      totalBytesSent += client.print("<td>");
        totalBytesSent += client.print("<input name='s");
        totalBytesSent += client.print(entry->id);
        totalBytesSent += client.print("' type='text' value='");
        totalBytesSent += client.print(entry->name);
        totalBytesSent += client.print("'/>");
      totalBytesSent += client.print("</td>");
      totalBytesSent += client.print("<td class='delete'>&#10006;</td>");
    totalBytesSent += client.println("</tr>");
  }

  totalBytesSent += client.println("</tbody><tbody>");
  totalBytesSent += client.println("<tr>");
    totalBytesSent += client.println("<th>Input temperature sensor</th>");
    totalBytesSent += client.println("<td colspan=2><select name='is'></th>");
    printSensorOptions(client, Config.getInputSensorId());
    totalBytesSent += client.println("</select></td>");
  totalBytesSent += client.print("</tr>");
  totalBytesSent += client.println("<tr>");
    totalBytesSent += client.println("<th>Flow temperature sensor</th>");
    totalBytesSent += client.println("<td colspan=2><select name='fs'></th>");
    printSensorOptions(client, Config.getFlowSensorId());
    totalBytesSent += client.println("</select></td>");
  totalBytesSent += client.print("</tr>");
  totalBytesSent += client.println("<tr>");
    totalBytesSent += client.println("<th>Return temperature sensor</th>");
    totalBytesSent += client.println("<td colspan=2><select name='rs'></th>");
    printSensorOptions(client, Config.getReturnSensorId());
    totalBytesSent += client.println("</select></td>");
  totalBytesSent += client.print("</tr>");

  totalBytesSent += client.println("</tbody><table>");
  totalBytesSent += client.println("<input type='submit' style='display:block; margin:0 auto;' value='Save Changes'/>");
  totalBytesSent += client.println("</form>");

  totalBytesSent += finishHttpHtmlResponse(client);
  return totalBytesSent;
}

size_t CMyWebServer::printSensorOptions(WiFiClient &client, const String &selectedSensor)
{
  int sensorCount = this->m_sensorMap->getCount();
  size_t totalBytesSent = 0;
  totalBytesSent += client.println("<option value=''>Not selected</option>>");
  for (int i = 0; i < sensorCount; i++)
  {
    SensorMapEntry *entry = (*m_sensorMap)[i];
    totalBytesSent += client.print("<option value='");
    totalBytesSent += client.print(entry->id);
    totalBytesSent += client.print(selectedSensor == entry->id ? "' selected>" : "'>");
    totalBytesSent += client.print(entry->name);
    totalBytesSent += client.println("</option>");
  }
  return totalBytesSent;
}

void CMyWebServer::processConfigPagePost(String postData) {

  // Parse the POST data into key-value pairs
  // Format: key1=value1&key2=value2&key3=value3
  String currentKey = "";
  String currentValue = "";
  bool parsingKey = true;
  bool processValue = false;
  int sensorIndex = 0;
  bool pidReconfigured = false;

  for (size_t i = 0; i < postData.length(); i++) {
    char c = postData[i];

    if (parsingKey) {
      if (c == '=') { // End of key, switch from parsing key to parsing value
        parsingKey = false;
      }
      else{
        currentKey += c;
      }
    }
    else {
      if (c == '&') { // End of value, process it
        processValue = true;
      }
      else {
        currentValue += c;
      }
    }
    // If this was the last character, we need to process the key-value pair in all cases
    if (i == (postData.length() - 1) && !currentKey.isEmpty()) processValue = true;

    if (processValue) {
      // URL decode the value (convert %XX to characters and + to space)
      URLCode decoder;
      decoder.urlcode = currentValue;
      decoder.urldecode();
      currentValue = decoder.strcode;

      // Process the key-value pair
      if (currentKey.startsWith("s")) {
        // Handle sensor name updates
        this->m_sensorMap->updateAtIndex(sensorIndex++, currentKey.substring(1), currentValue);
      }
      else if (currentKey == "tft") {
        Config.setFlowTargetTemp(currentValue.toFloat());
      }
      else if (currentKey == "pg") {
        Config.setProportionalGain(currentValue.toDouble());
      }
      else if (currentKey == "is") {
        Config.setIntegralSeconds(currentValue.toDouble());
      }
      else if (currentKey == "ds") {
        Config.setDerivativeSeconds(currentValue.toDouble());
      }
      else if (currentKey == "is") {
        Config.setInputSensorId(currentValue);
        pidReconfigured = true;
      }
      else if (currentKey == "fs") {
        Config.setFlowSensorId(currentValue);
        pidReconfigured = true;
      }
      else if (currentKey == "rs") {
        Config.setReturnSensorId(currentValue);
        pidReconfigured = true;
      }

      // Reset for next pair
      currentKey = "";
      currentValue = "";
      parsingKey = true;
      processValue = false;
    }
  }
  this->m_sensorMap->removeFromIndex(sensorIndex); // Remove any remaining sensors
  Config.saveToSdCard(*this->m_sd, *this->m_sdMutex, "/config.json", *this->m_sensorMap);
  if (pidReconfigured) this->m_valveManager->loadConfig();
  Config.print(MyLog);
}

