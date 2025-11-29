#include "../MyWebServer.h"
#include "MyLog.h"


size_t CMyWebServer::sendFileChunk(WebResponseContext *context, uint8_t *buffer, size_t maxLen, size_t fromPosition) {

  // Acquire a lock. If we can't we end this file.
  if (!this->m_sdMutex->lock()) return 0;

  // Open the file and stop if we can't
  FsFile file = this->m_sd->open(context->fileName, O_RDONLY);
  if (!file || !file.isOpen()) {
    this->m_sdMutex->unlock();
    delete context;
    return 0;
  }

  // Fill the buffer
  int bytesRead = 0;
  if (file.seek(fromPosition)) {
    bytesRead = file.readBytes((char *)buffer, maxLen);
  }

  // Close the file until the next time round and free the lock
  file.close();
  this->m_sdMutex->unlock();
  if (bytesRead == 0) {
    delete context;
  }

  return bytesRead;
}

void CMyWebServer::respondWithFileContents(AsyncWebServerRequest *request, const String &fileName) {

  String contentType = fileName.endsWith(".csv") ? "text/csv" : "text/plain";

  // Allocate a streaming context on heap so it outlives this function
  WebResponseContext *ctx = new WebResponseContext();
  ctx->fileName = fileName;

  // Set up the "sendFileChunk" callback that reads data from the file
  auto filler = [this, ctx](uint8_t *buffer, size_t maxLen, size_t fromPosition) -> size_t {
    return this->sendFileChunk(ctx, buffer, maxLen, fromPosition);
  };

  AsyncWebServerResponse *response = request->beginChunkedResponse(contentType, filler);
  request->send(response);
}

void CMyWebServer::respondWithDirectory(AsyncWebServerRequest *request, const String &path) {
  if (this->m_sdMutex->lock()) {
    AsyncResponseStream *response = startHttpHtmlResponse(request);
    FsFile dir = this->m_sd->open("/", O_RDONLY);
    if (!dir.isOpen()) {
      response->println("Unable to open directory");
    }
    else if (!dir.isDir()) {
      response->print(path);
      response->println(" is not a directory");
    }
    else {
      FsBaseFile file; 
      char nameBuffer[100];
      response->println("<table class='list'><thead><tr><th>File</th><th>Size (kb)</th></tr></thead><tbody>");
      while (file.openNext(&dir, O_RDONLY)) {
        // indent for dir level
        if (!file.isHidden()) {
          file.getName(nameBuffer,sizeof(nameBuffer));
          nameBuffer[sizeof(nameBuffer)-1] = '\0';
          response->print("<tr><td><a href='/");
          response->print(nameBuffer);
          response->print("'>");
          response->print(nameBuffer);
          response->println("</a></td><td class='right'>");
          response->print(file.fileSize() / 1024.0);
          response->println("</a></td></tr>");
        }
        file.close();
      }
    }
    response->println("</tbody></table>");
    this->m_sdMutex->unlock();
    finishHttpHtmlResponse(response);
    request->send(response);
  }
}



