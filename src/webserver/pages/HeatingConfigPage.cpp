#include "../MyWebServer.h"
#include "ESPmDNS.h"
#include "NeohubManager.h"
#include <vector>
#include "../HtmlGenerator.h"


void CMyWebServer::respondWithHeatingConfigPage(AsyncWebServerRequest *request) {
  AsyncResponseStream *response= this->startHttpHtmlResponse(request);

  HtmlGenerator html(response);

  html.element("form", "action='config' method='post'", [this, &html]{
    html.blockLayout([this, &html]{

      html.block("Room Thermostats", [this, &html]{
        html.select("name='ts'", [this, &html]{
          generateThermostatOptions(html, 0);
        });
      });

      html.block("Control Paraneters", [&html]{
        html.fieldTable( [&html] {
          html.fieldTableRow("Flow Setpoint", [&html]{
            html.fieldTableInput("name='tft' type='text' class='num-3em'", Config.getFlowTargetTemp(), 1);
            html.print("<td>&deg;C</td>");
          });
          html.fieldTableRow("Proportional Gain", [&html]{
            html.fieldTableInput("name='pid_pg' type='text' class='num-3em'", Config.getProportionalGain(), 1);
            html.print("<td>% per &deg;C error</td>");
          });
          html.fieldTableRow("Integral Term", [&html]{
            html.fieldTableInput("name='pid_is' type='text' class='num-3em'", Config.getIntegralSeconds(), 1);
            html.print("<td>seconds to correct 1% per &deg;C error</td>");
          });
          html.fieldTableRow("Derivative Term", [&html]{
            html.fieldTableInput("name='pid_ds' type='text' class='num-3em'", Config.getDerivativeSeconds(), 1);
            html.print("<td>seconds</td>");
          });
          html.fieldTableRow("Valve Direction", [&html]{
            html.fieldTableSelect("colspan=2", "name='vd'", [&html]{
              html.option("0", "Standard (clockwise)",  Config.getValveInverted() == false);
              html.option("1", "Inverted (conter-clockwise)",  Config.getValveInverted() == false);
            });
          });
        });
      });

      html.block("Sensor Names", [this, &html]{
        html.fieldTable( [this, &html] {
          html.print("<thead></tr><th>Sensor Id</th><th>Name</th></tr></thead>");
          html.element("tbody", "class='sensorList'", [this, &html]{
            int sensorCount = this->m_sensorMap->getCount();
            for (int i = 0; i < sensorCount; i++) {
              SensorMapEntry * entry = (*m_sensorMap)[i];
              String fieldParameter = "name='s" + entry->id + "' type='text'";
              html.fieldTableRow(entry->id.c_str(), "class='handle'", [this, &html, fieldParameter, entry]{
                html.fieldTableInput(fieldParameter.c_str(), entry->name.c_str());
                html.print("<td class='delete-row'>&#10006;</td>");
              });
            }
          });
        });
      });

      html.block("Sensor Functions", [this, &html]{
        html.fieldTable( [this, &html] {
          html.fieldTableRow("Input temperature", [this, &html]{
            html.fieldTableSelect("name='is'", [this, &html]{
              this->generateSensorOptions(html, Config.getInputSensorId());
            });
          });
          html.fieldTableRow("Flow temperature", [this, &html]{
            html.fieldTableSelect("name='fs'", [this, &html]{
              this->generateSensorOptions(html, Config.getFlowSensorId());
            });
          });
          html.fieldTableRow("Return temperature", [this, &html]{
            html.fieldTableSelect("name='rs'", [this, &html]{
              this->generateSensorOptions(html, Config.getReturnSensorId());
            });
          });
        });
      });

    }); // block layout
    html.print("<input type='submit' class='save-button' value='Save Changes'/>");
  }); // </form>

  finishHttpHtmlResponse(response);
  request->send(response);
}

void CMyWebServer::generateSensorOptions(HtmlGenerator &html, const String &selectedSensor)
{
  int sensorCount = this->m_sensorMap->getCount();
  html.option("", "Not Selected", false);
  for (int i = 0; i < sensorCount; i++)
  {
    SensorMapEntry *entry = (*m_sensorMap)[i];
    html.option(entry->id.c_str(), entry->name.c_str(), /* selected: */ selectedSensor == entry->id);
  }
}

void CMyWebServer::generateThermostatOptions(HtmlGenerator &html, int selectedThermostat)
{
  const std::vector<NeohubThermostatData> &thermostats = NeohubManager.getThermostatData();
  html.option("", "Not Selected", false);
  for (auto &i: thermostats)
  {
    html.option(String(i.id).c_str(), i.name.c_str(), /* selected: */ selectedThermostat == i.id);
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

void CMyWebServer::processHeatingConfigPagePost(AsyncWebServerRequest *request) {

  int sensorIndex = 0;
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
  respondWithHeatingConfigPage(request);

}

