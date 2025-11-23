#include <arduino.h>
#include "MyWebServer.h"
#include "MyLog.h"      // Lopgging to serial and, if available, SD card
#include <MyConfig.h> 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task.h" 
#include "freertos/idf_additions.h"

CMyWebServer MyWebServer;

CMyWebServer::CMyWebServer() : m_server(80) {
}

void CMyWebServer::setup(SdFs *sd, MyMutex *sdMutex, SensorMap *sensorMap, ValveManager * valveManager, SensorManager *sensorManager) {
  this->m_sd = sd;
  this->m_sdMutex = sdMutex;
  this->m_sensorMap = sensorMap;
  this->m_valveManager = valveManager;
  this->m_sensorManager = sensorManager;
  this->m_server.onNotFound([this](AsyncWebServerRequest *r) { this->processWebRequest(r); });
  this->m_server.begin();
}

String methodToString(WebRequestMethodComposite m) {
  switch (m) {
    case HTTP_GET:     return "GET";
    case HTTP_POST:    return "POST";
    case HTTP_PUT:     return "PUT";
    case HTTP_DELETE:  return "DELETE";
    case HTTP_OPTIONS: return "OPTIONS";
    case HTTP_PATCH:   return "PATCH";
    case HTTP_HEAD:    return "HEAD";
    default:           return "??? (" + String(m) + ")";
  }
}

// Process a single pending web reques
void CMyWebServer::processWebRequest(AsyncWebServerRequest *request) {

  WebRequestMethodComposite method = request->method();
  String url = request->url();

  if (method == HTTP_POST) {
    if (url == "/config") {
      processConfigPagePost(request);
    }
  }
  else if (method != HTTP_GET) {
    respondWithError(request, 400, "Unsupported Method: " + methodToString(method) + " on URL " + url);
  }
  else if (url == "/") {
    respondWithDirectory(request, "/");
  }
  else if (url == "/monitor") {
    respondWithMonitorPage(request);
  }
  else if (url == "/panic") {
    abort(); // Force a crash to test crash logging
  }
  else if (url == "/config") {
    respondWithConfigPage(request);
  }
  else if (url == "/tasks") {
    respondWithTaskList(request);
  }
  else {
    respondWithFileContents(request, url);
  }
}

void CMyWebServer::respondWithError(AsyncWebServerRequest *request, int code, const String &messageText) {
  AsyncWebServerResponse* r = request->beginResponse(code, "text/plain", messageText);
  request->send(r);
}

AsyncResponseStream *CMyWebServer::startHttpHtmlResponse(AsyncWebServerRequest *request, int refreshRate) {
  AsyncResponseStream* response = request->beginResponseStream("text/html");
  response->setCode(200);
  if (refreshRate > 0) response->addHeader("Refresh", String(refreshRate));
  response->println("<!DOCTYPE HTML>");
  response->println("<html>");
  response->println("  <head>");
  response->println("    <link rel='icon' href='data:,'>");
  response->println("    <script src='https://code.jquery.com/jquery-3.7.1.min.js'></script>");
  response->println("    <script src='https://code.jquery.com/ui/1.14.1/jquery-ui.min.js'></script>");
  response->println("    <script>");
  response->println("      function fixedWidthHelper(e, tr) {");
  response->println("        const $orig = tr.children();");
  response->println("        const $helper = tr.clone();");
  response->println("        $helper.children().each(function(i) {");
  response->println("          $(this).width($orig.eq(i).width());");
  response->println("        });");
  response->println("        return $helper;");
  response->println("      }");
  response->println("      $(function() {");
  response->println("        $('.delete').on('click', function(e) {");
  response->println("          e.preventDefault();");
  response->println("          $(this).closest('tr').remove();");
  response->println("        });");
  response->println("        $('.sensorList').sortable({");
  response->println("          handle: '.handle',");
  response->println("          helper: fixedWidthHelper,");
  response->println("          placeholder: 'sortable-placeholder',");
  response->println("          axis: 'y'");
  response->println("        }).disableSelection();");
  response->println("      });");
  response->println("    </script>");
  response->println("    <style>");
  response->println("      table { margin: 0 auto; font-size: 12pt; border-collapse: collapse; }");
  response->println("      table th { background: rgba(0,0,0,.1); }");
  response->println("      table tbody th, table thead th:first-child { text-align: left; }");
  response->println("      table th, table td { padding: .5em; border: 1px solid lightgrey; }");
  response->println("      .delete { width: 21px; cursor: pointer; text-align: center; color: red; }");
  response->println("      .handle { cursor: move; }");
  response->println("      .sortable-placeholder { background: #f5f5f5; visibility: visible !important; }");
  response->println("    </style>");
  response->println("  </head>");
  return response;
}

void CMyWebServer::finishHttpHtmlResponse(AsyncResponseStream *response) {
  response->println("</html>");
}


