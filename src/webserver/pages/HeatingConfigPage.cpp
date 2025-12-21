#include <vector>

#include "HtmlGenerator.h"
#include "../MyWebServer.h"
#include "ESPmDNS.h"
#include "NeohubManager.h"
#include "ValveManager.h"

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

      html.block("Heating Parameters", [this, &html]{
        html.fieldTable( [this, &html] {
          html.fieldTableRow("Room Setpoint", [&html]{
            html.fieldTableInput("name='room-setpoint' type='text' class='num-3em'", Config.getRoomSetpoint(), 1);
            html.print("<td>&deg;C</td>");
          });
          html.fieldTableRow("Proportional Gain", [&html]{
            html.fieldTableInput("name='room-pg' type='text' class='num-3em'", Config.getRoomProportionalGain(), 1);
            html.print("<td>&deg;C flow per &deg;C room temperature</td>");
          });
          html.fieldTableRow("Integral Time", [&html]{
            html.fieldTableInput("name='room-is' type='text' class='num-3em'", Config.getRoomIntegralMinutes(), 1);
            html.print("<td>minutes for 1x proportional gain</td>");
          });
        });
      });

      html.block("Manifold Configuration", [this, &html]{
        html.fieldTable( [this, &html] {
          html.fieldTableRow("Flow Range", [&html]{
            html.fieldTableInput("name='flow-setpoint-min' type='text' class='num-3em'", Config.getFlowMinSetpoint(), 1);
            html.print("<td>&mdash;");
            html.input("name='flow-setpoint-max' type='text' class='num-3em'", Config.getFlowMaxSetpoint(), 1);
            html.print("&deg;C</td>");
          });
          html.fieldTableRow("Proportional Gain", [&html]{
            html.fieldTableInput("name='flow-pg' type='text' class='num-3em'", Config.getFlowProportionalGain(), 1);
            html.print("<td>% per &deg;C flow temperature</td>");
          });
          html.fieldTableRow("Integral Time", [&html]{
            html.fieldTableInput("name='flow-is' type='text' class='num-3em'", Config.getFlowIntegralSeconds(), 1);
            html.print("<td>seconds for 1x proportional gain</td>");
          });
          html.fieldTableRow("Valve Direction", [&html]{
            html.fieldTableSelect("colspan=2", "name='valve-direction'", [&html]{
              html.option("0", "Standard (clockwise)",  Config.getFlowValveInverted() == false);
              html.option("1", "Inverted (counter-clockwise)",  Config.getFlowValveInverted() == true);
            });
          });
        });
      });

      html.block("Sensor Names",
        [&html] {
          html.print("<a class='sensor-scan' href='#' onclick='sendCommand({command:\"SensorScan\"})'>Scan</a>");
        },
        [this, &html]{
          html.fieldTable( [this, &html] {
            html.print("<thead></tr><th>Sensor Id</th><th>Temp.</th><th>Name</th><th class='delete-header'></th></tr></thead>");
            html.element("tbody", "class='dragDropList'", [this, &html]{
              int sensorCount = SensorMap.getCount();
              for (int i = 0; i < sensorCount; i++) {
                SensorMapEntry * entry = SensorMap[i];
                String fieldParameter = "name='s-" + entry->id + "' type='text'";
                html.fieldTableRow(entry->id.c_str(), "class='handle'", [this, &html, fieldParameter, entry]{
                  html.print("<td id='"); html.print(entry->id.c_str()); html.print("-temp' class='has-data'>");
                  float temperature = COneWireManager::SENSOR_NOT_FOUND;
                  OneWireSensor * sensor = OneWireManager.getSensor(entry->id.c_str());
                  if (sensor) temperature = sensor->calibratedTemperature();
                  if (temperature == COneWireManager::SENSOR_NOT_FOUND) html.print("???");
                  if (temperature > -50) html.print(String(temperature, 1).c_str());
                  html.print("</td>");
                  html.fieldTableInput(fieldParameter.c_str(), entry->name.c_str());
                  html.print("<td class='delete-row'></td>");
                });
              }
            });
          });
        }
      );

      html.block("Sensor Functions", [this, &html]{
        html.fieldTable( [this, &html] {
          html.fieldTableRow("Input temperature", [this, &html]{
            html.fieldTableSelect("name='sensor-input'", [this, &html]{
              this->generateSensorOptions(html, Config.getInputSensorId());
            });
          });
          html.fieldTableRow("Flow temperature", [this, &html]{
            html.fieldTableSelect("name='sensor-flow'", [this, &html]{
              this->generateSensorOptions(html, Config.getFlowSensorId());
            });
          });
          html.fieldTableRow("Return temperature", [this, &html]{
            html.fieldTableSelect("name='sensor-return'", [this, &html]{
              this->generateSensorOptions(html, Config.getReturnSensorId());
            });
          });
        });
      });

    }); // block layout
    html.footer();

    html.print("<button type='submit' class='call-to-action-button save-button'>Save Changes</button>");
  }); // </form>

  html.print("<script>setInterval(monitorPage_refreshData, 5000)</script>");
  finishHttpHtmlResponse(response);
  request->send(response);
}

void CMyWebServer::generateSensorOptions(HtmlGenerator &html, const String &selectedSensor)
{
  int sensorCount = SensorMap.getCount();
  html.option("", "Not Selected", false);
  for (int i = 0; i < sensorCount; i++)
  {
    SensorMapEntry *entry = SensorMap[i];
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
  bool pidReconfigured = false;       // Flag to determine if the PID controller ocnfiguration
                                      // has to be reloaded
  bool flowRangeReconfigured = false; // Flag to determine if the flow range in the
                                      // controller has to be updated

  NeohubManager.clearActiveZones();
  NeohubManager.clearMonitoredZones();

  int count = request->params();
  for (int i = 0; i < count; i++) {
    const AsyncWebParameter* p = request->getParam(i);
    if (!p->isPost()) continue;   // Ignore parameters that are not postback parameters
    const String & key = p->name();

    // Process the key-value pair
    if (key.startsWith("s-")) {
      // Handle sensor name updates
      SensorMap.updateAtIndex(sensorIndex++, key.substring(2), p->value());
    }
    else if (key == "room-setpoint") {
      if (update(Config.getRoomSetpoint(), &CConfig::setRoomSetpoint, p->value().toFloat())) {
        ValveManager.setRooomSetpoint(p->value().toFloat());
      }
    }
    else if (key == "room-pg") {
      pidReconfigured |= update(Config.getRoomProportionalGain(), &CConfig::setRoomProportionalGain, p->value().toFloat());
    }
    else if (key == "room-is") {
      pidReconfigured |= update(Config.getRoomIntegralMinutes(), &CConfig::setRoomIntegralMinutes, p->value().toFloat());
    }
    else if (key == "flow-setpoint-min") {
      flowRangeReconfigured |= update(Config.getFlowMinSetpoint(), &CConfig::setFlowMinSetpoint, p->value().toFloat());
    }
    else if (key == "flow-setpoint-max") {
      flowRangeReconfigured |= update(Config.getFlowMaxSetpoint(), &CConfig::setFlowMaxSetpoint, p->value().toFloat());
    }
    else if (key == "flow-pg") {
      pidReconfigured |= update(Config.getFlowProportionalGain(), &CConfig::setFlowProportionalGain, p->value().toFloat());
    }
    else if (key == "flow-is") {
      pidReconfigured |= update(Config.getFlowIntegralSeconds(), &CConfig::setFlowIntegralSeconds, p->value().toFloat());
    }
    else if (key == "valve-direction") {
      pidReconfigured |= update(Config.getFlowValveInverted(), &CConfig::setFlowValveInverted, (bool) p->value().toInt());
    }
    else if (key == "sensor-input") {
      Config.setInputSensorId(p->value());
    }
    else if (key == "sensor-flow") {
      Config.setFlowSensorId(p->value());
    }
    else if (key == "sensor-return") {
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

  SensorMap.removeFromIndex(sensorIndex); // Remove any remaining sensors
  Config.save();
  if (pidReconfigured) {
    ValveManager.loadConfig();
    // includes loading the flow range
  }
  else if (flowRangeReconfigured) {
    ValveManager.setFlowSetpointRange(Config.getFlowMinSetpoint(), Config.getFlowMaxSetpoint());
  }

  Config.print(MyLog);

  // After processing POST, respond with the config page again
  respondWithHeatingConfigPage(request);

}

