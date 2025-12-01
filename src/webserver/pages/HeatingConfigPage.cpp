#include "../MyWebServer.h"
#include "ESPmDNS.h"
#include "NeohubManager.h"
#include <vector>
#include "../HtmlGenerator.h"


void CMyWebServer::respondWithHeatingConfigPage(AsyncWebServerRequest *request) {
  AsyncResponseStream *response= this->startHttpHtmlResponse(request);

  HtmlGenerator html(response);
  html.navbar(NavbarPage::Config);

  html.element("form", "method='post'", [this, &html]{
    html.blockLayout([this, &html] {

      html.block("Room Zones", "class='content flex-wrap'", [this, &html]{
        html.fieldTable( [this, &html] {
          html.print("<thead></tr><th style='width: 21px'></th><th style='min-width: 8em'>Active</th><th class='delete-header'></th></tr></thead>");
          html.element("tbody", "class='dragDropListAcross'", [this, &html]{
            html.element("tr", "class='template-row'", [this,&html]{
              html.print("<td class='handle seq'/>0</td><td>");
              html.select("name='zone_a'", [this, &html]{
                generateZoneOptions(html, 0);
              });
              html.print("</td>");
              html.print("<td class='delete-row'></td>");
            });
            int i = 0;
            for (NeohubZone z: NeohubManager.getActiveZones()) {
              html.element("tr", [this, &html, i, z]{
                html.print("<td class='handle seq'/>");
                html.print(String(i+1).c_str());
                html.print("</td>");
                html.fieldTableSelect("name='zone_a'", [this, &html, z]{
                  this->generateZoneOptions(html, z.id);
                });
                html.print("<td class='delete-row'></td>");
              });
              i++;
            }
            html.print("<tr class='add-row'><td colspan=99><div></div></td></tr>");
          });
        });
        html.fieldTable( [this, &html] {
          html.print("<thead></tr><th style='width: 21px'></th><th style='min-width: 8em'>Monitored</th><th class='delete-header'></th></tr></thead>");
          html.element("tbody", "class='dragDropListAcross'", [this, &html]{
            html.element("tr", "class='template-row'", [this,&html]{
              html.print("<td class='handle seq'/>0</td><td>");
              html.select("name='zone_m'", [this, &html]{
                generateZoneOptions(html, 0);
              });
              html.print("</td>");
              html.print("<td class='delete-row'></td>");
            });
            int i = 0;
            for (NeohubZone z: NeohubManager.getMonitoredZones()) {
              html.element("tr", [this, &html, i, z]{
                html.print("<td class='handle seq'/>");
                html.print(String(i+1).c_str());
                html.print("</td>");
                html.fieldTableSelect("name='zone_m'", [this, &html, z]{
                  this->generateZoneOptions(html, z.id);
                });
                html.print("<td class='delete-row'></td>");
              });
              i++;
            }
            html.print("<tr class='add-row'><td colspan=99><div></div></td></tr>");
          });
        });
      });

      html.block("Heating Parameters", [&html]{
        html.fieldTable( [&html] {
          html.fieldTableRow("Room Setpoint", [&html]{
            html.fieldTableInput("name='r_sp' type='text' class='num-3em'", Config.getRoomSetpoint(), 1);
            html.print("<td>&deg;C</td>");
          });
          html.fieldTableRow("Proportional Gain", [&html]{
            html.fieldTableInput("name='r_pg' type='text' class='num-3em'", Config.getRoomProportionalGain(), 1);
            html.print("<td>&deg;C flow per &deg;C error</td>");
          });
          html.fieldTableRow("Integral Term", [&html]{
            html.fieldTableInput("name='r_is' type='text' class='num-3em'", Config.getRoomIntegralSeconds(), 1);
            html.print("<td>seconds to correct 1&deg;C flow per &deg;C error</td>");
          });
        });
      });

      html.block("Manifold Configuration", [&html]{
        html.fieldTable( [&html] {
          html.fieldTableRow("Flow Setpoint", [&html]{
            html.fieldTableInput("name='f_sp' type='text' class='num-3em'", Config.getFlowSetpoint(), 1);
            html.print("<td>&deg;C</td>");
          });
          html.fieldTableRow("Proportional Gain", [&html]{
            html.fieldTableInput("name='f_pg' type='text' class='num-3em'", Config.getProportionalGain(), 1);
            html.print("<td>% per &deg;C error</td>");
          });
          html.fieldTableRow("Integral Term", [&html]{
            html.fieldTableInput("name='f_is' type='text' class='num-3em'", Config.getIntegralSeconds(), 1);
            html.print("<td>seconds to correct 1% per &deg;C error</td>");
          });
          html.fieldTableRow("Derivative Term", [&html]{
            html.fieldTableInput("name='f_ds' type='text' class='num-3em'", Config.getDerivativeSeconds(), 1);
            html.print("<td>seconds</td>");
          });
          html.fieldTableRow("Valve Direction", [&html]{
            html.fieldTableSelect("colspan=2", "name='vd'", [&html]{
              html.option("0", "Standard (clockwise)",  Config.getValveInverted() == false);
              html.option("1", "Inverted (counter-clockwise)",  Config.getValveInverted() == true);
            });
          });
        });
      });

      html.block("Sensor Names", [this, &html]{
        html.fieldTable( [this, &html] {
          html.print("<thead></tr><th>Sensor Id</th><th>Name</th><th class='delete-header'></th></tr></thead>");
          html.element("tbody", "class='dragDropList'", [this, &html]{
            int sensorCount = this->m_sensorMap->getCount();
            for (int i = 0; i < sensorCount; i++) {
              SensorMapEntry * entry = (*m_sensorMap)[i];
              String fieldParameter = "name='s" + entry->id + "' type='text'";
              html.fieldTableRow(entry->id.c_str(), "class='handle'", [this, &html, fieldParameter, entry]{
                html.fieldTableInput(fieldParameter.c_str(), entry->name.c_str());
                html.print("<td class='delete-row'></td>");
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

void CMyWebServer::generateZoneOptions(HtmlGenerator &html, int selectedZone)
{
  const std::vector<NeohubZoneData> &zones = NeohubManager.getZoneData();
  html.option("", "Not Selected", false);
  for (auto &i: zones)
  {
    html.option(String(i.zone.id).c_str(), i.zone.name.c_str(), /* selected: */ selectedZone == i.zone.id);
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

  NeohubManager.clearActiveZones();
  NeohubManager.clearMonitoredZones();

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
    else if (key == "r_sp") {
      if (update(Config.getRoomSetpoint(), &CConfig::setRoomSetpoint, p->value().toFloat())) {
        // update the valve manager here...
      }
    }
    else if (key == "r_pg") {
      pidReconfigured |= update(Config.getRoomProportionalGain(), &CConfig::setRoomProportionalGain, p->value().toFloat());
    }
    else if (key == "r_is") {
      pidReconfigured |= update(Config.getRoomIntegralSeconds(), &CConfig::setRoomIntegralSeconds, p->value().toFloat());
    }
    else if (key == "f_sp") {
      if (update(Config.getFlowSetpoint(), &CConfig::setFlowSetpoint, p->value().toFloat())) {
        this->m_valveManager->setSetpoint(Config.getFlowSetpoint());
      }
    }
    else if (key == "f_pg") {
      pidReconfigured |= update(Config.getProportionalGain(), &CConfig::setProportionalGain, p->value().toFloat());
    }
    else if (key == "f_is") {
      pidReconfigured |= update(Config.getIntegralSeconds(), &CConfig::setIntegralSeconds, p->value().toFloat());
    }
    else if (key == "f_ds") {
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
    else if (key == "zone_a") {
      int id = p->value().toInt();
      NeohubZoneData *z = NeohubManager.getZoneData(id);
      if (z) {
        NeohubManager.addActiveZone(z->zone);
      };
    }
    else if (key == "zone_m") {
      int id = p->value().toInt();
      NeohubZoneData *z = NeohubManager.getZoneData(id);
      if (z) {
        if (NeohubManager.hasActiveZone(z->zone.id)) break;
        NeohubManager.addMonitoredZone(z->zone);
      }
    }
  }

  this->m_sensorMap->removeFromIndex(sensorIndex); // Remove any remaining sensors
  Config.saveToSdCard(*this->m_sd, *this->m_sdMutex, "/config.json", *this->m_sensorMap, NeohubManager);
  if (pidReconfigured) this->m_valveManager->loadConfig();
  Config.print(MyLog);

  // After processing POST, respond with the config page again
  respondWithHeatingConfigPage(request);

}

