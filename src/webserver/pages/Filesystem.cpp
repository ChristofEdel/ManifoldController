#include "../MyWebServer.h"
#include "MyLog.h"

size_t CMyWebServer::respondWithFileContents(WiFiClient &client, const String &fileName) {
  size_t totalBytesSent = 0;
  if (this->m_sdMutex->lock()) {
    FsFile f = this->m_sd->open(fileName, O_RDONLY);
    if (!f.isOpen()) {
      totalBytesSent += respondWithError(client, "404 NOT FOUND", fileName.c_str());
      this->m_sdMutex->unlock();
    }
    else {
      totalBytesSent += respondWithFileContentsWithLoggingAndUnlock(client, f, fileName.endsWith(".csv") ? "text/csv" : "text/plain");
    }
  }
  return totalBytesSent;
}

size_t CMyWebServer::respondWithFileContentsWithLoggingAndUnlock(WiFiClient &client, FsFile &file, const char *contentType) {
  size_t totalBytesSent = 0;
  size_t fileSize = file.fileSize();
  totalBytesSent += startHttpResponse(client, "200 OK", contentType, fileSize);
  uint8_t buffer[4000]; // 4000 character chunks

  long int totalFileReadMillis = 0;
  size_t totalFileBytes = 0;
  long int overallStartMillis = millis();

  while (file.available() && client.connected()) {

    // Read from the file and measure how much we have read and how long it took
    long int chunkReadStartMillis = millis();

    size_t bytesRead = file.read(buffer, sizeof(buffer));
    totalFileReadMillis += (millis() - chunkReadStartMillis);
    totalFileBytes += bytesRead;

    // If we have reached the end (or there is an error), stop
    if (bytesRead <= 0) {
      break;
    }

    // If there is some data, sent it to tje client 
    size_t bytesSent = client.write(buffer, bytesRead);
    if (bytesSent <= 0) break;
    totalBytesSent += bytesSent;
  }
  file.close();
  this->m_sdMutex->unlock();

  MyWebLog.print(totalFileReadMillis);
  MyWebLog.print(" ms for reading ");
  MyWebLog.print(totalFileBytes);
  MyWebLog.print(" bytes, ");
  MyWebLog.print((totalFileBytes / 1024.0) / (totalFileReadMillis / 1000.0));
  MyWebLog.println("kb/s");

  long int dataSendMilis = millis() - overallStartMillis - totalFileReadMillis;
  MyWebLog.print(dataSendMilis);
  MyWebLog.print(" ms for sending ");
  MyWebLog.print(totalBytesSent);
  MyWebLog.print(" bytes, ");
  MyWebLog.print((totalBytesSent / 1024.0) / (dataSendMilis / 1000.0));
  MyWebLog.println("kb/s");

  return totalBytesSent;
}

size_t CMyWebServer::respondWithDirectory(WiFiClient &client, const char * path) {
  if (this->m_sdMutex->lock()) {
    size_t totalBytesSent = 0;
    totalBytesSent += startHttpHtmlResponse(client);
    FsFile dir = this->m_sd->open("/", O_RDONLY);
    if (!dir.isOpen()) {
      totalBytesSent += client.println("Unable to open directory");
    }
    else if (!dir.isDir()) {
      totalBytesSent += client.print(path);
      totalBytesSent += client.println(" is not a directory");
    }
    else {
      FsBaseFile file; 
      char nameBuffer[100];
      totalBytesSent += client.println("<table><thead><tr><th>File</th><th>Size (kb)</th></tr></thead><tbody>");
      while (file.openNext(&dir, O_RDONLY)) {
        // indent for dir level
        if (!file.isHidden()) {
          file.getName(nameBuffer,sizeof(nameBuffer));
          nameBuffer[sizeof(nameBuffer)-1] = '\0';
          totalBytesSent += client.print("<tr><td><a href='/");
          totalBytesSent += client.print(nameBuffer);
          totalBytesSent += client.print("'>");
          totalBytesSent += client.print(nameBuffer);
          totalBytesSent += client.println("</a></td><td>");
          totalBytesSent += client.print(file.fileSize() / 1024.0);
          totalBytesSent += client.println("</a></td></tr>");
        }
        file.close();
      }
    }
    totalBytesSent += client.println("</tbody></table>");
    this->m_sdMutex->unlock();
    totalBytesSent += finishHttpHtmlResponse(client);
    return totalBytesSent;
  }
  return 0;
}



