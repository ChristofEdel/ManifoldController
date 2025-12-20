#include "HtmlGenerator.h"
#include "../MyWebServer.h"
#include "MyLog.h"
#include "EspTools.h"
#include "NeohubManager.h"
#include "StringTools.h"
#include <LittleFS.h>
#include <SdFat.h>

static bool ensureLittleFs() {
    static bool littleFsInitialised = false;
    if (!littleFsInitialised) {
        littleFsInitialised = LittleFS.begin(false, "/littlefs", 10, "ffat");
    }
    return littleFsInitialised;
}

void CMyWebServer::processLittleFsFileRequest(AsyncWebServerRequest* request)
{
    ensureLittleFs();

    String fileName = request->url();
    if (!fileName.startsWith("/littlefs")) {
        request->send(404, "text/plain", "File not found");
        return;
    }
    fileName = fileName.substring(9);

    File f = LittleFS.open(fileName, "r");
    bool exists = (bool)f;
    bool isDir  = exists && f.isDirectory();
    bool isFile = exists && !f.isDirectory();
    f.close();

    // Errors unless it is a file or directoy
    if (!exists) {
        this->respondWithError(request, 404, StringPrintf("File '%s' not found").c_str());
        return;
    }
    if (!isDir && !isFile) {
        this->respondWithError(request, 404, StringPrintf("'%s' is not a file or directory").c_str());
        return;
    }

    // Respond as appropriate
    if (isDir) {
        respondWithLittleFsDirectory(request, fileName);
    }
    else {
        respondWithLittleFsFileContents(request, fileName);
    }
}


size_t CMyWebServer::sendLittleFsFileChunk(WebResponseContext* context, uint8_t* buffer, size_t maxLen, size_t fromPosition)
{
    // Open the file and stop if we can't
    File file = LittleFS.open(context->fileName, "r");
    if (!file) {
        delete context;
        return 0;
    }

    // Fill the buffer
    int bytesRead = 0;
    if (file.seek(fromPosition)) {
        bytesRead = file.read(buffer, maxLen);
    }

    // Close the file until the next time round and free the lock
    file.close();
    if (bytesRead == 0) {
        delete context;
    }

    return bytesRead;
}


void CMyWebServer::respondWithLittleFsFileContents(AsyncWebServerRequest* request, const String& fileName)
{
    ensureLittleFs();

    // Allocate a streaming context on heap so it outlives this function
    WebResponseContext* ctx = new WebResponseContext();
    ctx->fileName = fileName;

    // Set up the "sendFileChunk" callback that reads data from the file
    auto filler = [this, ctx](uint8_t* buffer, size_t maxLen, size_t fromPosition) -> size_t {
        return this->sendLittleFsFileChunk(ctx, buffer, maxLen, fromPosition);
    };

    // Start a response
    AsyncWebServerResponse* response = request->beginChunkedResponse(getMimeType(fileName), filler);
    response->addHeader("Cache-Control", "public, max-age=31536000, immutable");
    if (fileName.endsWith(".gz")) {
        response->addHeader("Content-Encoding", "gzip");
    }
    request->send(response);
}

struct DirectoryEntry {
    String name;
    bool isDirectory;
    double sizeKb;

    DirectoryEntry(File& file)
    {
        name = file.name();
        isDirectory = file.isDirectory();
        sizeKb = file.size() / 1024.0;
    }
};

void CMyWebServer::respondWithLittleFsDirectory(AsyncWebServerRequest* request, const String& path)
{
    ensureLittleFs();

    // REad the directory information
    std::vector<DirectoryEntry> entries;

    // Collect all files in the directory
    File dir = LittleFS.open("/");
    if (!dir) {
        this->respondWithError(request, 404, "Unable to open directory");
        return;
    }

    if (!dir.isDirectory()) {
        dir.close();
        this->respondWithError(request, 500, "Not a directory");
        return;
    }

    for (File file = dir.openNextFile(); file; file = dir.openNextFile()) {
        entries.push_back(DirectoryEntry(file));
        file.close();
    }
    dir.close();

    // Sort by name, with directories on top

    auto cmp = [](const DirectoryEntry& a, const DirectoryEntry& b) {
        if (a.isDirectory != b.isDirectory) return a.isDirectory;  // directories first
        return a.name > b.name;
    };
    std::sort(entries.begin(), entries.end(), cmp);

    // Now print HTML
    AsyncResponseStream* response = startHttpHtmlResponse(request);
    HtmlGenerator html(response);
    html.navbar(NavbarPage::Files);
    response->println("<div class='navbar-border'></div>");
    response->println("<table class='list'><thead><tr><th>File</th><th>Size (kb)</th></tr></thead><tbody>");
    for (DirectoryEntry& e : entries) {
        response->print("<tr>");
        response->printf("<td><a href='/littlefs/%s'>%s</a></td>", e.name.c_str(), e.name.c_str());
        response->printf("<td class='right'>%.1f</td>", e.sizeKb);
    }
    response->println("</tbody></table>");
    finishHttpHtmlResponse(response);
    request->send(response);
}
