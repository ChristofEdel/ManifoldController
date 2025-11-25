#include "../MyWebServer.h"
#include "ESPmDNS.h"

void CMyWebServer::respondWithConfigPage(AsyncWebServerRequest *request) {
  AsyncResponseStream *response = this->startHttpHtmlResponse(request);
  response->println("<form action='config' method='post'>");
  
  response->println("<table><tbody>");

  response->print("<tr><th colspan=99>Heating</th></tr>");

  response->print("<tr>");
    response->print("<th>Flow Setpoint</th>");
    response->print("<td colspan=2><input name='tft' type='text' value='");
    response->print(Config.getFlowTargetTemp(),1);
    response->print("'/></td>");
  response->print("</tr>");

  response->println("<tr><th colspan=99>PID Controller</th></tr>");

  response->print("<tr>");
    response->print("<th>Proportional Gain</th>");
    response->print("<td colspan=2><input name='pid_pg' type='text' value='");
    response->print(Config.getProportionalGain(),1);
    response->print("'/></td>");
  response->print("</tr>");

  response->print("<tr>");
    response->print("<th>Integral Seconds</th>");
    response->print("<td colspan=2><input name='pid_is' type='text' value='");
    response->print(Config.getIntegralSeconds(),1);
    response->print("'/></td>");
  response->print("</tr>");

  response->print("<tr>");
    response->print("<th>Derivative Seconds</th>");
    response->print("<td colspan=2><input name='pid_ds' type='text' value='");
    response->print(Config.getDerivativeSeconds(),1);
    response->print("'/></td>");
  response->print("</tr>");

  response->print("<tr>");
    response->print("<th>Valve Direction</th>");
    response->println("<td colspan=2><select name='vd'>");
    response->printf("  <option value='0'%s>Standard (clockwise)</option>", Config.getValveInverted() == false ? " selected" : "");
    response->printf("  <option value='1'%s>Inverted (conter-clockwise)</option>", Config.getValveInverted() == true ? " selected" : "");
    response->print("'</select></td>");
  response->print("</tr>");

  response->print("<tr><th colspan=99>System</th></tr>");

  response->print("<tr>");
    response->print("<th>Hostname</th>");
    response->print("<td colspan=2><input name='hostname' type='text' value='");
    response->print(Config.getHostname());
    response->print("'/></td>");
  response->print("</tr>");


  response->println("<tr><th colspan=99>Sensor Config</th></tr>");

  response->print("<tr>");
    response->print("<th>Sensor ID</th>");
    response->print("<th colspan=2>Name</th>");
  response->println("</tr>");
  response->println("</tbody><tbody class='sensorList'>");

  int sensorCount = this->m_sensorMap->getCount();
  for (int i = 0; i < sensorCount; i++) {
    SensorMapEntry * entry = (*m_sensorMap)[i];
    response->print("<tr>");
      response->print("<th class='handle'>");
        response->print(entry->id);
      response->print("</th>");
      response->print("<td>");
        response->print("<input name='s");
        response->print(entry->id);
        response->print("' type='text' value='");
        response->print(entry->name);
        response->print("'/>");
      response->print("</td>");
      response->print("<td class='delete-row'>&#10006;</td>");
    response->println("</tr>");
  }

  response->println("</tbody><tbody>");
  response->println("<tr>");
    response->println("<th>Input temperature sensor</th>");
    response->println("<td colspan=2><select name='is'>");
    printSensorOptions(response, Config.getInputSensorId());
    response->println("</select></td>");
  response->print("</tr>");
  response->println("<tr>");
    response->println("<th>Flow temperature sensor</th>");
    response->println("<td colspan=2><select name='fs'>");
    printSensorOptions(response, Config.getFlowSensorId());
    response->println("</select></td>");
  response->print("</tr>");
  response->println("<tr>");
    response->println("<th>Return temperature sensor</th>");
    response->println("<td colspan=2><select name='rs'>");
    printSensorOptions(response, Config.getReturnSensorId());
    response->println("</select></td>");
  response->print("</tr>");

  response->println("</tbody><table>");
  response->println("<input type='submit' style='display:block; margin:0 auto;' value='Save Changes'/>");
  response->println("</form>");

  finishHttpHtmlResponse(response);
  request->send(response);
}

void CMyWebServer::printSensorOptions(AsyncResponseStream *response, const String &selectedSensor)
{
  int sensorCount = this->m_sensorMap->getCount();
  response->println("<option value=''>Not selected</option>");
  for (int i = 0; i < sensorCount; i++)
  {
    SensorMapEntry *entry = (*m_sensorMap)[i];
    response->print("<option value='");
    response->print(entry->id);
    response->print(selectedSensor == entry->id ? "' selected>" : "'>");
    response->print(entry->name);
    response->println("</option>");
  }
}

bool update(double oldValue, void (CConfig::*setter)(double), double newValue) {
  if (oldValue == newValue) return false;
  (Config.*setter)(newValue);
  return true;
}
bool update(bool oldValue, void (CConfig::*setter)(bool), bool newValue) {
  if (oldValue == newValue) return false;
  (Config.*setter)(newValue);
  return true;
}

void CMyWebServer::processConfigPagePost(AsyncWebServerRequest *request) {

  int sensorIndex = 0;
  bool hostnameChanged = false;     // Flag if we have to redirect to the new hostname
  bool pidReconfigured = false;     // Flag to determine if the PID controller ocnfiguration
                                    // has to be reloaded

  int count = request->params();
  for (int i = 0; i < count; i++) {
    const AsyncWebParameter* p = request->getParam(i);
    if (!p->isPost()) continue;   // Ignore parameters that are not postback parameters
    const String & key = p->name();

    // Process the key-value pair
    if (key.startsWith("s")) {
      // Handle sensor name updates
      this->m_sensorMap->updateAtIndex(sensorIndex++, key.substring(1), p->value());
    }
    else if (key == "hostname") {
      Config.setHostname(p->value());
      MyWiFi.setHostname(p->value());
      hostnameChanged = true;
    }
    else if (key == "tft") {
      if (update(Config.getFlowTargetTemp(), &CConfig::setFlowTargetTemp, p->value().toFloat())) {
        this->m_valveManager->setSetpoint(Config.getFlowTargetTemp());
      }
    }
    else if (key == "pid_pg") {
      pidReconfigured |= update(Config.getProportionalGain(), &CConfig::setProportionalGain, p->value().toFloat());
    }
    else if (key == "pid_is") {
      pidReconfigured |= update(Config.getIntegralSeconds(), &CConfig::setIntegralSeconds, p->value().toFloat());
    }
    else if (key == "pid_ds") {
      pidReconfigured |= update(Config.getDerivativeSeconds(), &CConfig::setDerivativeSeconds, p->value().toFloat());
    }
    else if (key == "vd") {
      pidReconfigured |= update(Config.getValveInverted(), &CConfig::setValveInverted, (bool) p->value().toInt());
    }
    else if (key == "is") {
      Config.setInputSensorId(p->value());
    }
    else if (key == "fs") {
      Config.setFlowSensorId(p->value());
    }
    else if (key == "rs") {
      Config.setReturnSensorId(p->value());
    }
  }

  this->m_sensorMap->removeFromIndex(sensorIndex); // Remove any remaining sensors
  Config.saveToSdCard(*this->m_sd, *this->m_sdMutex, "/config.json", *this->m_sensorMap);
  if (pidReconfigured) this->m_valveManager->loadConfig();
  Config.print(MyLog);

  // After processing POST, respond with the config page again
  if (!hostnameChanged) {
    respondWithConfigPage(request);
  }
  else {
    AsyncWebServerResponse *response = request->beginResponse(302);  
    response->addHeader("Location", "http://" + Config.getHostname() + ".local" + request->url());  
    request->send(response);
  }

}

