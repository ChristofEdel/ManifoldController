#include "../MyWebServer.h"
#include "ESPmDNS.h"
#include "StringTools.h"
#include "MqttManager.h"
#include "WeatherDataManager.h"
#include "NeohubZoneManager.h"

extern void republishConfiguration();

void CMyWebServer::respondWithSystemConfigPage(AsyncWebServerRequest *request) {
  AsyncResponseStream *response = this->startHttpHtmlResponse(request);
  HtmlGenerator html(response);

  html.navbar(NavbarPage::System);

  html.element("form", "method='post'", [this, &html]{
    html.blockLayout([this, &html]{

      html.block("Names", [this, &html]{
        html.fieldTable( [this, &html] {
          html.fieldTableRow("Display Name", [&html]{
            html.fieldTableInput("name='displayname'",Config.getName().c_str());
          });
          html.fieldTableRow("Hostname", [&html]{
            html.fieldTableInput("name='hostname'", Config.getHostname().c_str());
          });
        });
      });

      html.block("MQTT Server", [this, &html]{
        html.fieldTable( [this, &html] {
          html.fieldTableRow("Host", [&html]{
            html.fieldTableInput("name='mqtt_host' style='width: 20em'", Config.getMqttHost().c_str());
          });
          html.fieldTableRow("Port", [&html]{
            html.fieldTableInput("name='mqtt_port' style='width: 20em'",Config.getMqttPort());
          });
          html.fieldTableRow("User Name", [&html]{
            html.fieldTableInput("name='mqtt_un' style='width: 20em'",Config.getMqttUsername().c_str());
          });
          html.fieldTableRow("Password", [&html]{
            html.fieldTableInput("name='mqtt_pw' style='width: 20em'",Config.getMqttPassword().c_str());
          });
        });
      });

      html.block("MQTT Topics", [this, &html]{
        html.fieldTable( [this, &html] {
          html.fieldTableRow("Neohub", [&html]{
            html.fieldTableInput("name='topic_nh' style='width: 20em'", Config.getMqttTopicNeohub().c_str());
          });
          html.fieldTableRow("Temperature", [&html]{
            html.fieldTableInput("name='topic_t' style='width: 20em'",Config.getMqttTopicTemperature().c_str());
          });
          html.fieldTableRow("Temperature Alive", [&html]{
            html.fieldTableInput("name='topic_t_k' style='width: 20em'",Config.getMqttTopicTemperatureKeepalive().c_str());
          });
        });
      });

      html.block("Valve Test", [this, &html]{
        html.fieldTable( [this, &html] {
          html.fieldTableRow("Manual Control", [&html]{
            html.element("td", "style='text-align: left'",  [&html] {
              html.printf("<input type='checkbox' id='valveControlManualCheckbox' %s>", ValveManager.valveUnderManualControl() ? "checked" : "");
            });
          });
          html.fieldTableRow("Position", [&html]{
            html.element("td", "style='text-align: left'", [&html] {
              int p = (int)(std::isnan(ValveManager.getValvePosition()) ? 0 : ValveManager.getValvePosition());
              html.printf("<input type='range' id='valveControlPositionSlider' min='0' max='100' value='%d' step='1'>", p);
              html.printf("<span id='valveControlPositionText'>%d</span>", p);
            });
          });
        });
      });

      html.block("Salus Valve Reset", [this, &html]{
        html.fieldTable( [this, &html] {
          html.fieldTableRow("Zone", [this, &html]{
            html.element("td", "style='text-align: left'", [this, &html]() {
              html.select("id='zoneToResetSelect'", [this, &html]{
                generateZoneOptions(html, 0);
              });
            });
          });
          html.fieldTableRow("Setpoint", [&html]{
            html.print("<td><div id='resetZoneSetpoint' type='button' class='zone-on'></div></td>");
          });
          html.fieldTableRow("", [&html]{
            html.print("<td><div class='reset-progress-bar'>");
            html.print("<div id='resetProgressIndicator' style='display: none'></div>");
            html.print("<div style='width: 60px' class='on'>30s</div>");
            html.print("<div style='width: 30px' class='off'>15s</div>");
            html.print("<div style='width: 30px' class='on'>15s</div>");
            html.print("<div style='width: 30px' class='off'>15s</div>");
            html.print("<div style='width: 180px' class='on'>90s</div>");
            html.print("<div style='width: 40px' class='off'>20s</div>");
            html.print("<div style='width: 40px; border-left: 1px solid green' class='on'>Auto</div>");
            html.print("</div></td>");
          });
          html.element("tr", [&html]() {
            html.element("th", "colspan=2", [&html]() {
              html.print("<button id='resetZoneStartButton' type='button' disabled>Start Reset Sequence</button>");
              html.print("<button id='resetZoneStopButton' type='button' style='display:none'>Stop</button>");
            });
          });
        });
      });

    }); // block layout
    html.footer();
    html.print("<button type='submit' class='call-to-action-button save-button'>Save Changes</button>");
  }); // </form>

  finishHttpHtmlResponse(response);
  request->send(response);
}

void CMyWebServer::processSystemConfigPagePost(AsyncWebServerRequest *request) {

  bool hostnameChanged      = false; // Flag if we have to redirect to the new hostname
  bool changesMade          = false; // Flag if any changes were nade and we need to save them
  bool reconnectMqtt        = false; // Flag if we have to reconnect to the MQTT server

  int count = request->params();
  for (int i = 0; i < count; i++) {
    const AsyncWebParameter* p = request->getParam(i);
    if (!p->isPost()) continue;   // Ignore parameters that are not postback parameters
    const String & key = p->name();

    if (key == "displayname" && p->value() != Config.getName()) {
      Config.setName(p->value());
      changesMade = true;
    }
    if (key == "hostname" && p->value() != Config.getHostname()) {
      Config.setHostname(p->value());
      changesMade = true;
      hostnameChanged = true;
    }
    if (key == "mqtt_host" && p->value() != Config.getMqttHost()) {
      Config.setMqttHost(p->value());
      reconnectMqtt = true;
      changesMade = true;
    }
    if (key == "mqtt_port" && p->value().toInt() != Config.getMqttPort()) {
      Config.setMqttPort(p->value().toInt());
      reconnectMqtt = true;
      changesMade = true;
    }
    if (key == "mqtt_un" && p->value() != Config.getMqttUsername()) {
      Config.setMqttUsername(p->value());
      reconnectMqtt = true;
      changesMade = true;
    }
    if (key == "mqtt_pw" && p->value() != Config.getMqttPassword()) {
      Config.setMqttPassword(p->value());
      reconnectMqtt = true;
      changesMade = true;
    }
    if (key == "topic_nh" && p->value() != Config.getMqttTopicNeohub()) {
      Config.setMqttTopicNeohub(p->value());
      reconnectMqtt = true;
      changesMade = true;
    }
    if (key == "topic_t" && p->value() != Config.getMqttTopicTemperature()) {
      Config.setMqttTopicTemperature(p->value());
      reconnectMqtt = true;
      changesMade = true;
    }
    if (key == "topic_t_k" && p->value() != Config.getMqttTopicTemperatureKeepalive()) {
      Config.setMqttTopicTemperatureKeepalive(p->value());
      reconnectMqtt = true;
      changesMade = true;
    }
  }

  if (changesMade) {
    Config.save();
    republishConfiguration();
  }

  if (reconnectMqtt) {
    if (!Config.getMqttHost().isEmpty()) {
      MqttManager.restart(
        StringPrintf("mqtt://%s:%d", Config.getMqttHost().c_str(), Config.getMqttPort()), 
        Config.getMqttUsername(), 
        Config.getMqttPassword()
      );
      NeohubZoneManager.subscribeZoneData(Config.getMqttTopicNeohub());
      WeatherDataManager.subscribeWeatherData(Config.getMqttTopicTemperature(), Config.getMqttTopicTemperatureKeepalive());
    }
    else {
      MqttManager.stop();
    }
  }

  // After processing POST, respond with the config page again
  if (!hostnameChanged) {
    respondWithSystemConfigPage(request);
  }
  else {
    AsyncWebServerResponse *response = request->beginResponse(302);  
    response->addHeader("Location", "http://" + Config.getHostname() + ".local" + request->url());  
    request->send(response);
    softwareReset(SW_RESET_HOSTNAME_CHANGED);
  }

}

