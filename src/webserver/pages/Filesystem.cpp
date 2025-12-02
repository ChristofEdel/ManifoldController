#include "../MyWebServer.h"
#include "../HtmlGenerator.h"
#include "MyLog.h"


size_t CMyWebServer::sendFileChunk(WebResponseContext *context, uint8_t *buffer, size_t maxLen, size_t fromPosition) {

  // Acquire a lock. If we can't we end this file.
  if (!this->m_sdMutex->lock(__PRETTY_FUNCTION__)) return 0;

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

struct DirectoryEntry {
    String name;
    uint16_t date;
    uint16_t time;
    bool isDirectory;
    double sizeKb;
    DirectoryEntry(FsFile &file) {
      char nameBuffer[64];
      file.getName(nameBuffer, sizeof(nameBuffer));
      name = nameBuffer;
      sizeKb = file.size()/1024.0;
      isDirectory= file.isDirectory();
      if (
        isDirectory
        || !file.getModifyDateTime(&date, &time)
      ) {
        date=0; time = 0;
      }
    }
    const char *formatDate(char *buf, size_t len) {
      if (date == 0) {
        buf[0] = '\0';
      }
      else {
        uint16_t y = ((date >> 9) & 0x7F) + 1980;
        uint8_t  m = (date >> 5) & 0x0F;
        uint8_t  d = date & 0x1F;

        uint8_t  hh = time >> 11;
        uint8_t  mm = (time >> 5) & 0x3F;
        uint8_t  ss = (time & 0x1F) * 2;

        snprintf(buf, len, "%02u/%02u/%04u %02u:%02u:%02u", d, m, y, hh, mm, ss);
      }
      return buf;
    }
};

void CMyWebServer::respondWithDirectory(AsyncWebServerRequest *request, const String &path) {

  // REad the directory information
  std::vector<DirectoryEntry> entries;
  if (this->m_sdMutex->lock(__PRETTY_FUNCTION__)) {
    // Collect all files in the directory

    FsFile dir = this->m_sd->open("/", O_RDONLY);
    if (!dir.isOpen()) {
      request->send(400, "text/plain", "Unable to open directory");
      return;
    }

    if (!dir.isDir()) {
      request->send(400, "text/plain", "Not a directory");
      dir.close();
      return;
    }

    FsFile file;
    while (file.openNext(&dir, O_RDONLY)) {
      if (file.isHidden()) continue;
      entries.push_back(DirectoryEntry(file));
      file.close();
    }
    dir.close();

    this->m_sdMutex->unlock();
  }

  // Sort by descending timestamp, with directories on top

  auto cmp = [](const DirectoryEntry &a, const DirectoryEntry &b)
  {
    if (a.isDirectory != b.isDirectory) return a.isDirectory; // directories first
    if (a.date != b.date) return a.date > b.date; // newest first
    if (a.time != b.time) return a.time > b.time; // newest first
    return a.name > b.name;
  };

  std::sort(entries.begin(), entries.end(), cmp);

  // Now print HTML
  AsyncResponseStream *response = startHttpHtmlResponse(request);
  HtmlGenerator html(response);
  html.navbar(NavbarPage::Files);
  response->println("<div class='navbar-border'></div>");
  response->println("<table class='list'><thead><tr><th>File</th><th>Size (kb)</th><th>Modified</th></tr></thead><tbody>");
  for (DirectoryEntry &e : entries)
  {
    response->print("<tr>");
    response->printf("<td><a href='/files/%s'>%s</a></td>", e.name.c_str(), e.name.c_str());
    response->printf("<td class='right'>%.0f</td>", e.sizeKb);

    char buf[25];
    response->printf("<td>%s</td>", e.formatDate(buf, sizeof(buf)));
    response->println("</tr>");
  }
  response->println("</tbody></table>");
  finishHttpHtmlResponse(response);
  request->send(response);
}
