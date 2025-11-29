#ifndef __MYWEBSERVER_H
#define __MYWEBSERVER_H

#include <SdFat.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "NeohubConnection.h"
#include "MyWifi.h"
#include "OneWireSensorManager.h" // OneWire sensor reading and management
#include "SensorMap.h"            // Sensor name mapping
#include "ValveManager.h"
#include "MyMutex.h"
#include "HtmlGenerator.h"


struct WebResponseContext {
  String fileName;
};

class CMyWebServer {
  private:
    AsyncWebServer m_server;
    SdFs *m_sd;
    MyMutex *m_sdMutex;
    SensorMap *m_sensorMap;
    ValveManager *m_valveManager;
    SensorManager *m_sensorManager;
  public:
    CMyWebServer(void);
    void setup(SdFs *sd, MyMutex *sdMutex, SensorMap *sensorMap, ValveManager *valveManager, SensorManager *sensorManager);

  private:
      // Simple responses
      void respondWithError(AsyncWebServerRequest *request, int code, const String &messageText);
      void respondWithString(AsyncWebServerRequest *request, const String &contentType, const char *s);

      // File server
      void processFileRequest(AsyncWebServerRequest *request);
      void respondWithDirectory(AsyncWebServerRequest *request, const String &path);
      void respondWithFileContents(AsyncWebServerRequest *request, const String &fileName);
      size_t sendFileChunk(WebResponseContext *context, uint8_t *buffer, size_t maxLen, size_t index);

      // Heatmiser Neohub pass-through
      void respondFromNeohub(AsyncWebServerRequest *request);

      // HTML pages - main functions
      AsyncResponseStream *startHttpHtmlResponse(AsyncWebServerRequest *request);
      void finishHttpHtmlResponse(AsyncResponseStream *response);

      void respondWithMonitorPage(AsyncWebServerRequest *request);

      void respondWithHeatingConfigPage(AsyncWebServerRequest *request);
      void processHeatingConfigPagePost(AsyncWebServerRequest *request);
      void respondWithSystemConfigPage(AsyncWebServerRequest *request);
      void processSystemConfigPagePost(AsyncWebServerRequest *request);
 
      void generateSensorOptions(HtmlGenerator &html, const String &selectedSensor);
      void generateThermostatOptions(HtmlGenerator &html, int selectedThermostat);
      const String &mapSensorName(const String &name) const;

      // json 
      void respondWithStatusData(AsyncWebServerRequest *response);

      // Other pages for debugging
      void respondWithTaskList(AsyncWebServerRequest *request);

      // Helper function
      static void assemblePostBody(
        AsyncWebServerRequest *request,
        uint8_t *data, size_t len,
        size_t index, size_t total
      );

};

extern CMyWebServer MyWebServer;

#endif
