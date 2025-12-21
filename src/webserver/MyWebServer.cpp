#include <arduino.h>
#include <NeohubConnection.h>
#include "MyWebServer.h"
#include "MyLog.h"      // Lopgging to serial and, if available, SD card
#include <MyConfig.h> 
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task.h" 
#include "freertos/idf_additions.h"
#include "styles_string.h"
#include "scripts_string.h"
#include "NeohubManager.h"
#include "EspTools.h"

CMyWebServer MyWebServer;

CMyWebServer::CMyWebServer() : m_server(80) {}

String methodToString(WebRequestMethodComposite m) 
{
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

void CMyWebServer::setup()
{
    this->m_server.on(AsyncURIMatcher::exact("/"),              HTTP_GET, [this](AsyncWebServerRequest *r) { this->respondWithMonitorPage(r); });

    // Filesystem
    this->m_server.on(AsyncURIMatcher::exact("/sdcard"),        HTTP_GET, [this](AsyncWebServerRequest *r) { this->respondWithDirectory(r, "/sdcard"); });
    this->m_server.on(AsyncURIMatcher::dir  ("/sdcard"),        HTTP_GET, [this](AsyncWebServerRequest *r) { this->processFileRequest(r); });
    this->m_server.on(AsyncURIMatcher::exact("/flash"),         HTTP_GET, [this](AsyncWebServerRequest *r) { this->respondWithDirectory(r, "/flash"); });
    this->m_server.on(AsyncURIMatcher::dir  ("/flash"),         HTTP_GET, [this](AsyncWebServerRequest *r) { this->processFileRequest(r); });
    this->m_server.on(AsyncURIMatcher::exact("/delete-file"),   HTTP_POST,[this](AsyncWebServerRequest *r) { this->processDeleteFileRequest(r); });

    // Static files
    this->m_server.on(AsyncURIMatcher::exact("/scripts.js"),    HTTP_GET, [this](AsyncWebServerRequest *r) { this->respondWithString(r, "text/javascript", SCRIPTS_JS_STRING); });
    this->m_server.on(AsyncURIMatcher::exact("/styles.css"),    HTTP_GET, [this](AsyncWebServerRequest *r) { this->respondWithString(r, "text/css", STYLES_CSS_STRING); });

    // Core dumps and other debug helpers
    this->m_server.on(AsyncURIMatcher::exact("/coredump.elf"),  HTTP_GET, [this](AsyncWebServerRequest *r) { this->processCoreDumpRequest(r); });
    this->m_server.on(AsyncURIMatcher::exact("/backtrace.txt"), HTTP_GET, [this](AsyncWebServerRequest *r) { this->processBacktraceRequest(r); });
    this->m_server.on(AsyncURIMatcher::exact("/messagelog.txt"),HTTP_GET, [this](AsyncWebServerRequest *r) { this->processMessageLogRequest(r); });
    this->m_server.on(AsyncURIMatcher::exact("/panic"),         HTTP_GET, [this](AsyncWebServerRequest *r) { softwareAbort(SW_RESET_PANIC_TEST); /* Force a crash to test crash logging */ });
    this->m_server.on(AsyncURIMatcher::exact("/reset"),         HTTP_GET, [this](AsyncWebServerRequest *r) { softwareReset(SW_RESET_USER_RESET); });

    // Web pages
    this->m_server.on(AsyncURIMatcher::exact("/monitor"),       HTTP_GET, [this](AsyncWebServerRequest *r) { this->respondWithMonitorPage(r); });
    this->m_server.on(AsyncURIMatcher::exact("/config-system"), HTTP_GET, [this](AsyncWebServerRequest *r) { this->respondWithSystemConfigPage(r); });
    this->m_server.on(AsyncURIMatcher::exact("/config-system"), HTTP_POST,[this](AsyncWebServerRequest *r) { this->processSystemConfigPagePost(r); });
    this->m_server.on(AsyncURIMatcher::exact("/config"),        HTTP_GET, [this](AsyncWebServerRequest *r) { this->respondWithHeatingConfigPage(r); });
    this->m_server.on(AsyncURIMatcher::exact("/config"),        HTTP_POST,[this](AsyncWebServerRequest *r) { this->processHeatingConfigPagePost(r); });
    this->m_server.on(AsyncURIMatcher::exact("/tasks"),         HTTP_GET, [this](AsyncWebServerRequest *r) { this->respondWithTaskList(r); });
    
    // JSON
    this->m_server.on(AsyncURIMatcher::exact("/data/status"),   HTTP_GET, [this](AsyncWebServerRequest *r) { this->respondWithStatusData(r); });
    this->m_server.on(AsyncURIMatcher::exact("/data/status"),   HTTP_OPTIONS, [this](AsyncWebServerRequest *r) { this->respondToOptionsRequest(r); });
    this->m_server.on(AsyncURIMatcher::exact("/command"),       HTTP_POST,[this](AsyncWebServerRequest *r) { this->executeCommand(r); }, nullptr, CMyWebServer::assemblePostBody);
    this->m_server.on(AsyncURIMatcher::exact("/command"),       HTTP_OPTIONS, [this](AsyncWebServerRequest *r) { this->respondToOptionsRequest(r); });
    this->m_server.on(AsyncURIMatcher::exact("/neohub"),        HTTP_POST,[this](AsyncWebServerRequest *r) { this->respondFromNeohub(r); }, nullptr, CMyWebServer::assemblePostBody);

    // Catch-all: no random file links
    this->m_server.on(AsyncURIMatcher::dir  ("/"),              HTTP_GET, [this](AsyncWebServerRequest *r) { this->respondWithError(r, 404, "File not found"); });
    this->m_server.onNotFound([this](AsyncWebServerRequest *r) { this->respondWithError(r, 400, "Unsupported Method: " + methodToString(r->method()) + " on URL " + r->url()); });
    this->m_server.begin();
}

void CMyWebServer::respondWithError(AsyncWebServerRequest* request, int code, const String& messageText)
{
    AsyncWebServerResponse* r = request->beginResponse(code, "text/plain", messageText);
    request->send(r);
}

void CMyWebServer::respondWithString(AsyncWebServerRequest* request, const String& contentType, const char* s)
{
    AsyncWebServerResponse *response = request->beginResponse(200, contentType, (const uint8_t *)s, strlen(s));
    request->send(response);
}

AsyncResponseStream* CMyWebServer::startHttpHtmlResponse(AsyncWebServerRequest* request)
{
    AsyncResponseStream* response = request->beginResponseStream("text/html");
    response->setCode(200);
    response->println("<!DOCTYPE HTML>");
    response->println("<html>");
    response->println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
    response->println("  <head>");
    response->println("    <link rel='icon' href='data:,'>");
    response->println("    <script src='https://code.jquery.com/jquery-3.7.1.min.js'></script>");
    response->println("    <script src='https://code.jquery.com/ui/1.14.1/jquery-ui.min.js'></script>");
    response->println("    <script src='/scripts.js'></script>");
    response->println("    <link rel='stylesheet' href='/styles.css'>");
    response->println("  </head>");
    response->println("  <body>");
    response->println("    <div class='page-div'>");
    return response;
}

void CMyWebServer::finishHttpHtmlResponse(AsyncResponseStream* response)
{
    response->println("</div></body></html>");
}

void CMyWebServer::respondFromNeohub(AsyncWebServerRequest* r)
{
    const int timeoutMillis = 2000;
    bool disconnected = false;
    int responseCode = 200;
    String responseString;

    String* body = (String*)r->_tempObject;
    r->_tempObject = nullptr; // take ownership of the body
    if (!body) {
        responseCode = 400;
        responseString = "{ \"error\": \"Empty request\" }";
    }
    else {
        responseString = NeohubManager.neohubCommand(*body, timeoutMillis);

        if (responseString == emptyString) {
            responseCode = 400;
            responseString = "{ \"error\": \"Unable to retrieve data from Neohub\" }";
        }
    }
    delete body;
    
    AsyncWebServerResponse* response = r->beginResponse(responseCode, "application/json", responseString);
    response->addHeader("Access-Control-Allow-Origin", "*");
    response->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    response->addHeader("Access-Control-Allow-Headers", "Content-Type");
    r->send(response);
}

void CMyWebServer::assemblePostBody(
    AsyncWebServerRequest* request,
    uint8_t* data, size_t len,
    size_t index, size_t total
)
{
    String* body;
    // First time round? create a sting for the body
    if (index == 0) {
        // first chunk
        body = new String();
        body->reserve(total);
        request->_tempObject = body;
    }
    // all others - continue with the previous string
    else {
        body = (String*)request->_tempObject;
    }
    // append what we just received
    body->concat((const char*)data, len);
}

String CMyWebServer::getMimeType(const String &fileName) {

    return 
        fileName.endsWith(".js.min") ? "application/javascript" : 
        fileName.endsWith(".js.min.gz") ? "application/javascript" : 
        fileName.endsWith(".css.min") ? "text/css" : 
        fileName.endsWith(".css.min.gz") ? "text/css" : 
        fileName.endsWith(".json") ? "application/json" : 
        fileName.endsWith(".csv") ? "text/plain" : 
        fileName.endsWith(".txt") ? "text/plain" : 
        "application/octet-stream";
}

