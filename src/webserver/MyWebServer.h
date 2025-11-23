#ifndef __MYWEBSERVER_H
#define __MYWEBSERVER_H

#include <SdFat.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "MyWifi.h"
#include "OneWireSensorManager.h" // OneWire sensor reading and management
#include "SensorMap.h"            // Sensor name mapping
#include "ValveManager.h"
#include "MyMutex.h"


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
      void processWebRequest(AsyncWebServerRequest *request);
      void respondWithFileContents(AsyncWebServerRequest *request, const String &fileName);
      size_t sendFileChunk(WebResponseContext *context, uint8_t *buffer, size_t maxLen, size_t index);
      void respondWithMonitorPage(AsyncWebServerRequest *request);

      void respondWithConfigPage(AsyncWebServerRequest *request);
      void processConfigPagePost(AsyncWebServerRequest *request);

      void printSensorOptions(AsyncResponseStream *response, const String &selectedSensor);
      void respondWithDirectory(AsyncWebServerRequest *request, const String &path);
      void respondWithError(AsyncWebServerRequest *request, int code, const String &messageText);
      void respondWitTracePage(AsyncWebServerRequest *request, bool);
      void respondWithTaskList(AsyncWebServerRequest *request);

      AsyncResponseStream *startHttpHtmlResponse(AsyncWebServerRequest *request, int refresh = 0);
      void finishHttpHtmlResponse(AsyncResponseStream *response);

      const String &mapSensorName(const String &name) const;

};

extern CMyWebServer MyWebServer;

#endif
