#include "../MyWebServer.h"
#include "ESPmDNS.h"

void CMyWebServer::respondWithSystemConfigPage(AsyncWebServerRequest *request) {
  AsyncResponseStream *response = this->startHttpHtmlResponse(request);
  response->println("<form action='config' method='post'>");
  
  response->println("<table><tbody>");

  response->print("<tr><th colspan=99>System</th></tr>");

  response->print("<tr>");
    response->print("<th>Hostname</th>");
    response->print("<td colspan=2><input name='hostname' type='text' value='");
    response->print(Config.getHostname());
    response->print("'/></td>");
  response->print("</tr>");

  response->println("</tbody><table>");
  response->println("<input type='submit' style='display:block; margin:0 auto;' value='Save Changes'/>");
  response->println("</form>");

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

