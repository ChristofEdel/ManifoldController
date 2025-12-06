#include "../MyWebServer.h"
#include "ESPmDNS.h"

void CMyWebServer::respondWithSystemConfigPage(AsyncWebServerRequest *request) {
  AsyncResponseStream *response = this->startHttpHtmlResponse(request);
  HtmlGenerator html(response);

  m_valveManager->resumeAutomaticValveControl();
  // Instead of showing the current value on the page, we show "automatic"
  // on the page and make sure the system behaves accordingly.

  html.navbar(NavbarPage::System);

  html.element("form", "method='post'", [this, &html]{
    html.blockLayout([this, &html]{

      html.block("Hostname", [this, &html]{
        html.input("name='hostname'", Config.getHostname().c_str());
      });

      html.block("Neohub", [this, &html]{
        html.fieldTable( [this, &html] {
          html.fieldTableRow("URL", [&html]{
            html.fieldTableInput("name='nh_url' style='width: 20em'", Config.getNeohubAddress().c_str());
          });
          html.fieldTableRow("Token", [&html]{
            html.fieldTableInput("name='nh_token' style='width: 20em'",Config.getNeohubToken().c_str());
          });
        });
      });

      html.block("Valve Test", [this, &html]{
        html.fieldTable( [this, &html] {
          html.fieldTableRow("Manual Control", [&html]{
            html.element("td", "style='text-align: left'", "<input type='checkbox' id='valveControlManual'>");
          });
          html.fieldTableRow("Position", [&html]{
            html.element("td", "style='text-align: left'", [&html] {
              html.print("<input type='range' id='valveControlPositionSlider' min='0' max='100' value='0'>");
              html.print("<span id='valveControlPositionValue'>0</span>");
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

void CMyWebServer::processSystemConfigPagePost(AsyncWebServerRequest *request) {

  bool hostnameChanged = false;     // Flag if we have to redirect to the new hostname
  bool changesMade     = false;     // Flag if any changes were nade and we need to save them
  bool reconnectNeohub = false;     // Flag if we have to reconnect to the neohub

  int count = request->params();
  for (int i = 0; i < count; i++) {
    const AsyncWebParameter* p = request->getParam(i);
    if (!p->isPost()) continue;   // Ignore parameters that are not postback parameters
    const String & key = p->name();

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
  }

  if (changesMade) {
    Config.saveToSdCard(*this->m_sd, *this->m_sdMutex, "/config.json", *this->m_sensorMap, NeohubManager);
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

