#ifndef __MYWEBSERVER_H
#define __MYWEBSERVER_H

#include <ESPAsyncWebServer.h>

#include "../tools/HtmlGenerator.h"
#include "MyWifi.h"
#include "NeohubConnection.h"
#include "OneWireManager.h"  // OneWire sensor reading and management
#include "SensorMap.h"       // Sensor name mapping
#include "ValveManager.h"

struct WebResponseContext {
    String fileName;
};

class SdFs;

class CMyWebServer {
  private:
    AsyncWebServer m_server;

  public:
    CMyWebServer(void);
    void setup();

  private:
    // Simple responses
    void respondWithError(AsyncWebServerRequest* request, int code, const String& messageText);
    void respondWithString(AsyncWebServerRequest* request, const String& contentType, const char* s);

    // File server
    void processFileRequest(AsyncWebServerRequest* request);
    void respondWithDirectory(AsyncWebServerRequest* request, const String& path);
    void respondWithFileContents(AsyncWebServerRequest* request, const String& fileName);
    size_t sendFileChunk(WebResponseContext* context, uint8_t* buffer, size_t maxLen, size_t index);
    void processDeleteFileRequest(AsyncWebServerRequest* request);

    void processCoreDumpRequest(AsyncWebServerRequest* request);
    void processBacktraceRequest(AsyncWebServerRequest* request);
    void processMessageLogRequest(AsyncWebServerRequest* request);

    // HTML pages - main functions
    AsyncResponseStream* startHttpHtmlResponse(AsyncWebServerRequest* request);
    void finishHttpHtmlResponse(AsyncResponseStream* response);

    void respondWithMonitorPage(AsyncWebServerRequest* request);

    void respondWithHeatingConfigPage(AsyncWebServerRequest* request);
    void processHeatingConfigPagePost(AsyncWebServerRequest* request);
    void respondWithSystemConfigPage(AsyncWebServerRequest* request);
    void processSystemConfigPagePost(AsyncWebServerRequest* request);

    void generateSensorOptions(HtmlGenerator& html, const String& selectedSensor);
    void generateZoneOptions(HtmlGenerator& html, int selectedThermostat);
    const String& mapSensorName(const String& name) const;

    // json
    void respondWithStatusData(AsyncWebServerRequest* response);
    void executeCommand(AsyncWebServerRequest* response);
    void respondToOptionsRequest(AsyncWebServerRequest* request);

    // Other pages for debugging
    void respondWithTaskList(AsyncWebServerRequest* request);

    // Helper function
    static void assemblePostBody(
        AsyncWebServerRequest* request,
        uint8_t* data, size_t len,
        size_t index, size_t total
    );
    static String getMimeType(const String& fileName);

};

extern CMyWebServer MyWebServer;

#endif
