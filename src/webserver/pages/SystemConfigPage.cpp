#include "../MyWebServer.h"
#include "ESPmDNS.h"

void CMyWebServer::respondWithSystemConfigPage(AsyncWebServerRequest *request) {
  AsyncResponseStream *response = this->startHttpHtmlResponse(request);
  HtmlGenerator html(response);

  html.navbar(NavbarPage::System);

  html.element("form", "method='post'", [this, &html]{
    html.blockLayout([this, &html]{

      html.block("Hostname", [this, &html]{
        html.input("name='hostname'", Config.getHostname().c_str());
      });

      html.block("Neohub", [this, &html]{
        html.fieldTable( [this, &html] {
          html.fieldTableRow("URL", [&html]{
            html.fieldTableInput("name='nh_url' style='width: 20em'", "xxx.xx.xx.xx");
          });
          html.fieldTableRow("Token", [&html]{
            html.fieldTableInput("name='nh_token' style='width: 20em'", "xxxxxx-xxxxxx-xxxx-xxxxx-xxxxxxxxxx");
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

  int sensorIndex = 0;
  bool hostnameChanged = false;     // Flag if we have to redirect to the new hostname

  int count = request->params();
  for (int i = 0; i < count; i++) {
    const AsyncWebParameter* p = request->getParam(i);
    if (!p->isPost()) continue;   // Ignore parameters that are not postback parameters
    const String & key = p->name();

    if (key == "hostname" && p->value() != Config.getHostname()) {
      Config.setHostname(p->value());
      MyWiFi.setHostname(p->value());
      hostnameChanged = true;
    }
  }

  if (hostnameChanged) {
    Config.saveToSdCard(*this->m_sd, *this->m_sdMutex, "/config.json", *this->m_sensorMap);
    Config.print(MyLog);
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

