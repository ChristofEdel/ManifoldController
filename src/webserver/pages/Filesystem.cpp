#include "HtmlGenerator.h"
#include "../MyWebServer.h"
#include "MyLog.h"
#include "EspTools.h"

#include <SdFat.h>

void CMyWebServer::processFileRequest(AsyncWebServerRequest *request) {
    
    String fileName = request->url();
    if (!fileName.startsWith("/files")) {
        request->send(404, "text/plain", "File not found");
        return;
    }
    fileName = fileName.substring(6);

    // Open the file briefly and check for existence and what it is
    if (!this->m_sdMutex->lock(__PRETTY_FUNCTION__)) {
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
        respondWithDirectory(request, fileName);
    }
    else {
        respondWithFileContents(request, fileName);
    }
}

void CMyWebServer::processCoreDumpRequest(AsyncWebServerRequest* request)
{
    auto coreDump = std::make_shared<Esp32CoreDump>();

    if (!coreDump->exists()) {
        request->send(404, "text/plain", "No core dump");
    }

    AsyncWebServerResponse* response = request->beginResponse(
        "application/octet-stream", 
        coreDump->size(),
        [coreDump](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
            if (index >= coreDump->size()) return 0;  // done

            size_t remaining = coreDump->size() - index;
            size_t toRead = (remaining < maxLen) ? remaining : maxLen;
            return coreDump->read(index, buffer, toRead);
        }
    );

    String fileName = request->url().substring(1);

    response->addHeader("Content-Disposition", "attachment; filename=\"" + fileName + "\"");
    request->send(response);
}

void CMyWebServer::processCrashLogRequest(AsyncWebServerRequest* request)
{
    Esp32CoreDump coreDump;
    if (!coreDump.exists()) {
        request->send(404, "text/plain", "No crash log");
    }

    AsyncResponseStream* response = request->beginResponseStream("text/plain");
    if (coreDump.writeBacktrace(*response)) {
        response->setCode(200);
    }
    else {
        response->setCode(500);
    }
    request->send(response);
}

void CMyWebServer::processMessageLogRequest(AsyncWebServerRequest* request)
{
    AsyncResponseStream* response = request->beginResponseStream("text/plain");
    response->setCode(200);
    NeohubManager.printLatestMessages(*response);
    request->send(response);
}

size_t CMyWebServer::sendFileChunk(WebResponseContext* context, uint8_t* buffer, size_t maxLen, size_t fromPosition)
{
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
        bytesRead = file.readBytes((char*)buffer, maxLen);
    }

    // Close the file until the next time round and free the lock
    file.close();
    this->m_sdMutex->unlock();
    if (bytesRead == 0) {
        delete context;
    }

    return bytesRead;
}

void CMyWebServer::respondWithFileContents(AsyncWebServerRequest* request, const String& fileName)
{
    String contentType = fileName.endsWith(".csv") ? "text/csv" : "text/plain";

    // Allocate a streaming context on heap so it outlives this function
    WebResponseContext* ctx = new WebResponseContext();
    ctx->fileName = fileName;

    // Set up the "sendFileChunk" callback that reads data from the file
    auto filler = [this, ctx](uint8_t* buffer, size_t maxLen, size_t fromPosition) -> size_t {
        return this->sendFileChunk(ctx, buffer, maxLen, fromPosition);
    };

    AsyncWebServerResponse* response = request->beginChunkedResponse(contentType, filler);
    request->send(response);
}

struct DirectoryEntry {
    String name;
    uint16_t date;
    uint16_t time;
    bool isDirectory;
    double sizeKb;
    DirectoryEntry(FsFile& file)
    {
        char nameBuffer[64];
        file.getName(nameBuffer, sizeof(nameBuffer));
        name = nameBuffer;
        sizeKb = file.size() / 1024.0;
        isDirectory = file.isDirectory();
        if (isDirectory || !file.getModifyDateTime(&date, &time)) {
            date = 0;
            time = 0;
        }
    }
    const char* formatDate(char* buf, size_t len)
    {
        if (date == 0) {
            buf[0] = '\0';
        }
        else {
            uint16_t y = ((date >> 9) & 0x7F) + 1980;
            uint8_t m = (date >> 5) & 0x0F;
            uint8_t d = date & 0x1F;

            uint8_t hh = time >> 11;
            uint8_t mm = (time >> 5) & 0x3F;
            uint8_t ss = (time & 0x1F) * 2;

            snprintf(buf, len, "%02u/%02u/%04u %02u:%02u:%02u", d, m, y, hh, mm, ss);
        }
        return buf;
    }
};

void CMyWebServer::respondWithDirectory(AsyncWebServerRequest* request, const String& path)
{
    // REad the directory information
    std::vector<DirectoryEntry> entries;
    if (this->m_sdMutex->lock(__PRETTY_FUNCTION__)) {
        // Collect all files in the directory

        FsFile dir = this->m_sd->open("/", O_RDONLY);
        if (!dir.isOpen()) {
            this->m_sdMutex->unlock();
            request->send(400, "text/plain", "Unable to open directory");
            return;
        }

        if (!dir.isDir()) {
            dir.close();
            request->send(400, "text/plain", "Not a directory");
            this->m_sdMutex->unlock();
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

    auto cmp = [](const DirectoryEntry& a, const DirectoryEntry& b) {
        if (a.isDirectory != b.isDirectory) return a.isDirectory;  // directories first
        if (a.date != b.date) return a.date > b.date;              // newest first
        if (a.time != b.time) return a.time > b.time;              // newest first
        return a.name > b.name;
    };

    std::sort(entries.begin(), entries.end(), cmp);

    // Now print HTML
    AsyncResponseStream* response = startHttpHtmlResponse(request);
    HtmlGenerator html(response);
    html.navbar(NavbarPage::Files);
    response->println("<div class='navbar-border'></div>");
    response->println("<table class='list'><thead><tr><th>File</th><th>Size (kb)</th><th>Modified</th><th class='delete-header'></th></tr></thead><tbody>");
    Esp32CoreDump coreDump;
    if (coreDump.exists()) {
        response->print("<tr>");
        if (coreDump.getFormat() == "elf") {
            response->printf("<td><a href='/coredump.elf'>coredump.elf</a></td>");
        }
        else {
            response->printf("<td><a href='/coredump.bin'>coredump.bin</a></td>");
        }
        response->printf("<td class='right'>%.1f</td>", coreDump.size() / 1024.0);
        response->printf("<td></td><td class='delete-file'></td></tr>");
    }
    for (DirectoryEntry& e : entries) {
        response->print("<tr>");
        response->printf("<td><a href='/files/%s'>%s</a></td>", e.name.c_str(), e.name.c_str());
        response->printf("<td class='right'>%.1f</td>", e.sizeKb);

        char buf[25];
        response->printf("<td>%s</td>", e.formatDate(buf, sizeof(buf)));
        response->println("<td class='delete-file'></td></tr>");
    }
    response->println("</tbody></table>");
    finishHttpHtmlResponse(response);
    request->send(response);
}

void CMyWebServer::processDeleteFileRequest(AsyncWebServerRequest* request)
{
    const AsyncWebParameter* p = request->getParam("filename", true);
    if (!p) {
        request->send(400, "text/plain", "missing filename");
        return;
    }

    String filename = p->value();

    // Process special file "coredump.*"
    if (filename == "coredump.bin" || filename == "coredump.elf") {
        Esp32CoreDump dump;
        if (!dump.exists()) request->send(200, "text/plain", "coredump does not exist");
        if (dump.remove()) {
            request->send(200, "text/plain", "coredump deleted");
        }
        else {
            request->send(500, "text/plain", "cannot delete coredump");
        }
    }

    else {
        if (filename.startsWith("/files")) filename = filename.substring(6);  // strip leading /files
        if (!filename.startsWith("/"))                                        // SdFat needs leading slash
            filename = "/" + filename;

        if (filename.indexOf("..") != -1) {  // prevent navigating up
            request->send(400, "text/plain", "relative navigation not permitted");
            return;
        }

        bool result = false;

        if (this->m_sdMutex->lock(__PRETTY_FUNCTION__)) {
            result = this->m_sd->remove(filename.c_str());
            this->m_sdMutex->unlock();
        }
        if (result)
            request->send(200, "text/plain", "file deleted");
        else
            request->send(500, "text/plain", "delete failed");
    }

}
