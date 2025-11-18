#ifndef __MYWEBSERVER_H
#define __MYWEBSERVER_H

#include <SdFat.h>
#include "MyWifi.h"
#include "OneWireSensorManager.h" // OneWire sensor reading and management
#include "SensorMap.h"            // Sensor name mapping
#include "ValveManager.h"
#include "MyMutex.h"

class CMyWebServer {
  private:
    WiFiServer m_server;
    SdFs *m_sd;
    MyMutex *m_sdMutex;
    SensorMap *m_sensorMap;
    ValveManager *m_valveManager;
    SensorManager *m_sensorManager;
  public:
    CMyWebServer(void);
    void setup(SdFs *sd, MyMutex *sdMutex, SensorMap *sensorMap, ValveManager *valveManager, SensorManager *sensorManager);
    int processWebRequests(int maxRequests = 1);
  private:
      bool processSingleWebRequest(void);
      void parseRequest(String &firstRequestLine, String &verb, String &url);

      size_t respondWithFileContents(WiFiClient &client, const String &fileName);
      size_t respondWithFileContentsWithLoggingAndUnlock(WiFiClient &client, FsFile &file, const char *contentType);
      size_t respondWithMonitorPage(WiFiClient &client);

      size_t respondWithConfigPage(WiFiClient &client);
      void processConfigPagePost(String postData);

      size_t printSensorOptions(WiFiClient &client, const String &selectedSensor);
      size_t respondWithDirectory(WiFiClient &client, const char *path);
      size_t respondWithError(WiFiClient &client, const char *codeAndText, const char *messageText);
      size_t respondWitTracePage(WiFiClient &client, bool);
      size_t respondWithTaskList(WiFiClient &client);

      size_t startHttpResponse(WiFiClient &client, const char *codeAndText, const char *contentType, size_t contentLength = (size_t)-1, const char **headerLines = 0, int refresh = 0);
      size_t startHttpHtmlResponse(WiFiClient &client, const char *codeAndText = "200 OK", int refresh = 0);
      size_t finishHttpHtmlResponse(WiFiClient &client);

      const String &mapSensorName(const String &name) const;

};

extern CMyWebServer MyWebServer;

#endif
