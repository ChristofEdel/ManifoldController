#include <arduino.h>
#include "MyWebServer.h"
#include "MyLog.h"      // Lopgging to serial and, if available, SD card
#include <URLCode.h>    // URL decoding / encoding
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
  this->m_server.begin();
}

// Process pending web requests until either no request is pending, or until the maximum
// number of requests has been reached
int CMyWebServer::processWebRequests(int maxRequests) {
  int requestCount = 0;

  while (requestCount < maxRequests) {
    if (!processSingleWebRequest()) break;
    requestCount++;
  }

  return requestCount;
}

// Process a single pending web request; return false if no request is pending and
// true if a request has been processed.
bool CMyWebServer::processSingleWebRequest() {

  // See if we have a request. Return if not
  WiFiClient client = this->m_server.available();
  if (!client) return false;
  if (!client.available()) return false;    // For clients returned prematurely - we ignore these
  size_t totalBytesSent = 0;
  unsigned long startMillis = 0;

  // if we have one, process it
  while (client.connected()) {  // while loop so we can "break" after we have responded
    if (client.available()) {
      String firstRequestLine = client.readStringUntil('\r');
      // skip the rest of the request headers
      while (client.available()) {
        String headerLine = client.readStringUntil('\r');
        if (client.peek() == '\n') { client.read(); } // consume '\n'
        if (headerLine == "\n" || headerLine == "\r\n" || headerLine.length() == 0) {
          break;
        }
      }

      String verb;
      URLCode url;

      parseRequest (firstRequestLine, verb, url.urlcode);
      url.urldecode();

      MyWebLog.print("HTTP Request: ");
      MyWebLog.println(firstRequestLine);

      startMillis = millis();
      totalBytesSent = 0;

      if (verb == "POST" && url.strcode == "/config") {
        // Handle POST data
        String postData = client.readStringUntil('\r');
        postData.replace('+', ' ');   // because URLcode does not handle '+' to space conversion
        MyWebLog.print("POST Data: '");
        MyWebLog.print(postData);
        MyWebLog.println("'");

        processConfigPagePost(postData);
        // After processing POST, respond with the config page again
        totalBytesSent += respondWithConfigPage(client);
        break;
      }
      else

      if (verb != "GET") {
        totalBytesSent += respondWithError(client, "400 BAD REQUEST", firstRequestLine.c_str());
        break;
      }
      else if (url.strcode == "/") {
        totalBytesSent += respondWithDirectory(client, "/");
        break;
      }
      else if (url.strcode == "/monitor") {
        totalBytesSent += respondWithMonitorPage(client);
        break;
      }
      else if (url.strcode == "/panic") {
        abort(); // Force a crash to test crash logging
        break;
      }
      else if (url.strcode == "/config") {
        totalBytesSent += respondWithConfigPage(client);
        break;
      }
      else if (url.strcode == "/tasks") {
        totalBytesSent += respondWithTaskList(client);
        break;
      }
      else {
        totalBytesSent += respondWithFileContents(client, url.strcode);
        break;
      }
    }
    delay(1);
  }

  // close the connection:
  client.stop();

  // Log request completion
  unsigned long totalMillis = millis() - startMillis;
  MyWebLog.print("Request completed - ");
  MyWebLog.print(totalMillis);
  MyWebLog.print(" ms for sending ");
  MyWebLog.print(totalBytesSent);
  MyWebLog.print(" bytes, ");
  MyWebLog.print((totalBytesSent / 1024.0) / (totalMillis / 1000.0));
  MyWebLog.println("kb/s");

  return true;
}


void CMyWebServer::parseRequest(String &firstRequestLine, String &verb, String &url) {
  if (firstRequestLine.length() == 0) {
    verb="";
    url="";
    return;
  }
  int firstBlank = firstRequestLine.indexOf(' ');
  int secondBlank = firstRequestLine.indexOf(' ', firstBlank < 0 ? 0 : firstBlank+1);
  if (firstBlank < 0) {
    verb=firstRequestLine;
    url="";
  }
  verb = firstRequestLine.substring(0, firstBlank);
  if (secondBlank < 0) {
    url=firstRequestLine.substring(firstBlank+1);
  }
  else {
    url=firstRequestLine.substring(firstBlank+1, secondBlank);
  }

  return;
}

size_t CMyWebServer::respondWithError(WiFiClient &client, const char * codeAndText, const char * messageText) {
  size_t totalBytesSent = 0;
  totalBytesSent += startHttpHtmlResponse(client, codeAndText);
  totalBytesSent += client.print(codeAndText);
  totalBytesSent += client.print(" - ");
  totalBytesSent += client.println(messageText);
  totalBytesSent += finishHttpHtmlResponse(client);
  return totalBytesSent;
}

size_t CMyWebServer::startHttpResponse(WiFiClient &client, const char * codeAndText, const char * contentType, size_t contentLength, const char **headerLines, int refreshRate) {
  size_t totalBytesSent = 0;
  totalBytesSent += client.print("HTTP/1.1 ");
  totalBytesSent += client.println(codeAndText);
  totalBytesSent += client.print("Content-Type: ");
  totalBytesSent += client.println(contentType);
  if (contentLength != (size_t) -1) {
    totalBytesSent += client.print("Content-Length: ");
    totalBytesSent += client.println(contentLength);
  }
  if(refreshRate > 0) {
    totalBytesSent += client.print("Refresh: ");
    totalBytesSent += client.println(refreshRate);
  }
  totalBytesSent += client.println("Connection: close");
  if (headerLines) {
    while (*headerLines != 0) {
      totalBytesSent += client.print(*headerLines++);
    }
  }
  totalBytesSent += client.println();
  return totalBytesSent;
}

size_t CMyWebServer::startHttpHtmlResponse(WiFiClient &client, const char * codeAndText, int refresh) {
  size_t totalBytesSent = 0;
  totalBytesSent += startHttpResponse(client, codeAndText, "text/html", -1, 0, refresh);
  totalBytesSent += client.println("<!DOCTYPE HTML>");
  totalBytesSent += client.println("<html>");
  totalBytesSent += client.println("  <head>");
  totalBytesSent += client.println("    <link rel='icon' href='data:,'>");
  totalBytesSent += client.println("    <script src='https://code.jquery.com/jquery-3.7.1.min.js'></script>");
  totalBytesSent += client.println("    <script src='https://code.jquery.com/ui/1.14.1/jquery-ui.min.js'></script>");
  totalBytesSent += client.println("    <script>");
  totalBytesSent += client.println("      function fixedWidthHelper(e, tr) {");
  totalBytesSent += client.println("        const $orig = tr.children();");
  totalBytesSent += client.println("        const $helper = tr.clone();");
  totalBytesSent += client.println("        $helper.children().each(function(i) {");
  totalBytesSent += client.println("          $(this).width($orig.eq(i).width());");
  totalBytesSent += client.println("        });");
  totalBytesSent += client.println("        return $helper;");
  totalBytesSent += client.println("      }");
  totalBytesSent += client.println("      $(function() {");
  totalBytesSent += client.println("        $('.delete').on('click', function(e) {");
  totalBytesSent += client.println("          e.preventDefault();");
  totalBytesSent += client.println("          $(this).closest('tr').remove();");
  totalBytesSent += client.println("        });");
  totalBytesSent += client.println("        $('.sensorList').sortable({");
  totalBytesSent += client.println("          handle: '.handle',");
  totalBytesSent += client.println("          helper: fixedWidthHelper,");
  totalBytesSent += client.println("          placeholder: 'sortable-placeholder',");
  totalBytesSent += client.println("          axis: 'y'");
  totalBytesSent += client.println("        }).disableSelection();");
  totalBytesSent += client.println("      });");
  totalBytesSent += client.println("    </script>");
  totalBytesSent += client.println("    <style>");
  totalBytesSent += client.println("      table { margin: 0 auto; font-size: 12pt; border-collapse: collapse; }");
  totalBytesSent += client.println("      table th { background: rgba(0,0,0,.1); }");
  totalBytesSent += client.println("      table tbody th, table thead th:first-child { text-align: left; }");
  totalBytesSent += client.println("      table th, table td { padding: .5em; border: 1px solid lightgrey; }");
  totalBytesSent += client.println("      .delete { width: 21px; cursor: pointer; text-align: center; color: red; }");
  totalBytesSent += client.println("      .handle { cursor: move; }");
  totalBytesSent += client.println("      .sortable-placeholder { background: #f5f5f5; visibility: visible !important; }");
  totalBytesSent += client.println("    </style>");
  totalBytesSent += client.println("  </head>");
  return totalBytesSent;
}

size_t CMyWebServer::finishHttpHtmlResponse(WiFiClient &client) {
  return client.println("</html>");
}


