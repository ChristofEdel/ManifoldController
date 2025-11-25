#include <arduino.h>
#include "MyWebServer.h"
#include "MyLog.h"      // Lopgging to serial and, if available, SD card
#include <MyConfig.h> 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task.h" 
#include "freertos/idf_additions.h"
#include "styles_string.h"
#include "scripts_string.h"

CMyWebServer MyWebServer;

CMyWebServer::CMyWebServer() : m_server(80) {
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

void CMyWebServer::setup(SdFs *sd, MyMutex *sdMutex, SensorMap *sensorMap, ValveManager * valveManager, SensorManager *sensorManager) {
  this->m_sd = sd;
  this->m_sdMutex = sdMutex;
  this->m_sensorMap = sensorMap;
  this->m_valveManager = valveManager;
  this->m_sensorManager = sensorManager;
  this->m_server.on("/", HTTP_GET, [this](AsyncWebServerRequest *r) { this->respondWithDirectory(r, "/"); });
  this->m_server.on("/monitor", HTTP_GET, [this](AsyncWebServerRequest *r) { this->respondWithMonitorPage(r); });
  this->m_server.on("/config", HTTP_GET, [this](AsyncWebServerRequest *r) { this->respondWithConfigPage(r); });
  this->m_server.on("/config", HTTP_POST, [this](AsyncWebServerRequest *r) { this->processConfigPagePost(r); });
  this->m_server.on("/tasks", HTTP_GET, [this](AsyncWebServerRequest *r) { this->respondWithTaskList(r); });
  this->m_server.on("/panic", HTTP_GET, [this](AsyncWebServerRequest *r) { abort(); /* Force a crash to test crash logging */ });
  this->m_server.on("/data/status", HTTP_GET, [this](AsyncWebServerRequest *r) { this->respondWithStatusData(r); });
  this->m_server.on("/scripts.js", HTTP_GET, [this](AsyncWebServerRequest *r) { this->respondWithString(r, "text/javascript", SCRIPTS_JS_STRING); });
  this->m_server.on("/styles.css", HTTP_GET, [this](AsyncWebServerRequest *r) { this->respondWithString(r, "text/css", STYLES_CSS_STRING); });
  this->m_server.on("/*", HTTP_GET, [this](AsyncWebServerRequest *r) { this->processFileRequest(r); });
  this->m_server.onNotFound([this](AsyncWebServerRequest *r) { this->respondWithError(r, 400, "Unsupported Method: " + methodToString(r->method()) + " on URL " + r->url()); });
  this->m_server.begin();
}

void CMyWebServer::processFileRequest(AsyncWebServerRequest *request) {

  String fileName = request->url();

  // Open the file briefly and check for existence and what it is
  if (!this->m_sdMutex->lock()) {
      request->send(400, "text/plain", "Unable to access SD card");
      return;
  }

  FsFile f = this->m_sd->open(fileName, O_RDONLY);
  bool exists = f.isOpen();
  bool isDir = f.isDir();
  bool isFile = f.isFile();
  f.close();
  this->m_sdMutex->unlock();

  // Errors unless it is a file or directoy
  if (!exists) {
    request->send(404, "text/plain", "File not found");
    return; 
  }
  if (!isDir && !isFile) {
    request->send(404, "text/plain", "Not a file or directory");
    return; 
  }

  // Respond as appropriate
  if (isDir) {
    respondWithFileContents(request, request->url());
  }
  else {
    respondWithDirectory(request, request->url());
  }
}

void CMyWebServer::respondWithError(AsyncWebServerRequest *request, int code, const String &messageText) {
  AsyncWebServerResponse* r = request->beginResponse(code, "text/plain", messageText);
  request->send(r);
}

void CMyWebServer::respondWithString(AsyncWebServerRequest *request, const String &contentType, const char *s) {
  AsyncWebServerResponse *response = request->beginResponse_P(200, contentType, (const uint8_t *)s, strlen(s));
  request->send(response);
}

AsyncResponseStream *CMyWebServer::startHttpHtmlResponse(AsyncWebServerRequest *request) {
  AsyncResponseStream* response = request->beginResponseStream("text/html");
  response->setCode(200);
  response->println("<!DOCTYPE HTML>");
  response->println("<html>");
  response->println("  <head>");
  response->println("    <link rel='icon' href='data:,'>");
  response->println("    <script src='https://code.jquery.com/jquery-3.7.1.min.js'></script>");
  response->println("    <script src='https://code.jquery.com/ui/1.14.1/jquery-ui.min.js'></script>");
  response->println("    <script src='scripts.js'></script>");
  response->println("    <link rel='stylesheet' href='styles.css'>");
  response->println("  </head>");
  return response;
}

void CMyWebServer::finishHttpHtmlResponse(AsyncResponseStream *response) {
  response->println("</html>");
}

void CMyWebServer::respondWithStatusData(AsyncWebServerRequest *request) {
  JsonDocument statusJson;

  statusJson["flowSetpoint"]    = m_valveManager->getSetpoint();
  statusJson["valvePosition"]   = m_valveManager->outputs.targetValvePosition;
  
  int sensorCount = this->m_sensorMap->getCount();

  for (int i = 0; i < sensorCount; i++) {
      SensorMapEntry * entry = (*this->m_sensorMap)[i];
      Sensor * sensor = m_sensorManager->getSensor(entry->id.c_str());
      if (sensor) {
        statusJson["sensors"][i]["id"] = entry->id;
        statusJson["sensors"][i]["name"] = entry->name;
        statusJson["sensors"][i]["temperature"] = sensor->calibratedTemperature();
        statusJson["sensors"][i]["readings"] = sensor->readings;
        statusJson["sensors"][i]["crcErrors"] = sensor->crcErrors;
        statusJson["sensors"][i]["noResponseErrors"] = sensor->noResponseErrors;
        statusJson["sensors"][i]["otherErrors"] = sensor->otherErrors;
        statusJson["sensors"][i]["failures"] = sensor->failures;
      }

  }

  String result;
  serializeJsonPretty(statusJson, result);

  AsyncWebServerResponse* response = request->beginResponse(200, "application/json", result);
  response->addHeader("Access-Control-Allow-Origin", "*");
  response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  response->addHeader("Access-Control-Allow-Headers", "Content-Type");
  request->send(response);
}
