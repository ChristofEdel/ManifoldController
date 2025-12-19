#include "../MyWebServer.h"
#include "NeohubManager.h"
#include "ESPmDNS.h"

void CMyWebServer::respondWithSystemConfigPage(AsyncWebServerRequest *request) {
  AsyncResponseStream *response = this->startHttpHtmlResponse(request);
  HtmlGenerator html(response);

  ValveManager.resumeAutomaticValveControl();
  // Instead of showing the current value on the page, we show "automatic"
  // on the page and make sure the system behaves accordingly.

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

      html.block("Neohub", [this, &html]{
        html.fieldTable( [this, &html] {
          html.fieldTableRow("URL", [&html]{
            html.fieldTableInput("name='nh_url' style='width: 20em'", Config.getNeohubAddress().c_str());
          });
          html.fieldTableRow("Token", [&html]{
            html.fieldTableInput("name='nh_token' style='width: 20em'",Config.getNeohubToken().c_str());
          });
          html.print("<tr><th colspan=2><a href='/messagelog.txt'>Message Log</a></th></tr>");
        });
      });

      html.block("Heating Controller", [this, &html]{
        html.fieldTable( [this, &html] {
          html.fieldTableRow("URL", [&html]{
            html.fieldTableInput("name='hc_url' style='width: 20em'", Config.getHeatingControllerAddress().c_str());
          });
        });
      });

      html.block("Valve Test", [this, &html]{
        html.fieldTable( [this, &html] {
          html.fieldTableRow("Manual Control", [&html]{
            html.element("td", "style='text-align: left'", "<input type='checkbox' id='valveControlManualCheckbox'>");
          });
          html.fieldTableRow("Position", [&html]{
            html.element("td", "style='text-align: left'", [&html] {
              html.print("<input type='range' id='valveControlPositionSlider' min='0' max='100' value='0'>");
              html.print("<span id='valveControlPositionText'>0</span>");
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

  bool hostnameChanged = false;     // Flag if we have to redirect to the new hostname
  bool changesMade     = false;     // Flag if any changes were nade and we need to save them
  bool reconnectNeohub = false;     // Flag if we have to reconnect to the neohub

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
      MyWiFi.setHostname(p->value());
      changesMade = true;
      hostnameChanged = true;
    }
    if (key == "nh_url" && p->value() != Config.getNeohubAddress()) {
      Config.setNeohubAddress(p->value());
      reconnectNeohub = true;
      changesMade = true;
    }
    if (key == "nh_token" && p->value() != Config.getNeohubToken()) {
      Config.setNeohubToken(p->value());
      changesMade = true;
    }
    if (key == "hc_url" && p->value() != Config.getHeatingControllerAddress()) {
      Config.setHeatingControllerAddress(p->value());
      changesMade = true;
    }
  }

  if (changesMade) {
    Config.saveToSdCard(*this->m_sd, *this->m_sdMutex, "/config.json");
    Config.print(MyLog);
  }

  if (reconnectNeohub) {
    NeohubManager.reconnect();
  }


  // After processing POST, respond with the config page again
  if (!hostnameChanged) {
    respondWithSystemConfigPage(request);
  }
  else {
    AsyncWebServerResponse *response = request->beginResponse(302);  
    response->addHeader("Location", "http://" + Config.getHostname() + ".local" + request->url());  
    request->send(response);
  }

}

