#include "HtmlGenerator.h"
#include "../MyWebServer.h"
#include "MyLog.h"
#include "EspTools.h"
#include "NeohubConnection.h"
#include "Filesystem.h"
#include "StringTools.h"
#include <dirent.h>   // DIR, opendir(), readdir(), closedir(), struct dirent

void CMyWebServer::processFileRequest(AsyncWebServerRequest *request) {
    
    String fileName = request->url();
    if (!fileName.startsWith("/sdcard") && !fileName.startsWith("/flash")) {
        request->send(404, "text/plain", "File not found");
        return;
    }

    // Open the file briefly and check for existence and what it is
    Filesystem.lock();
    File f = Filesystem.open(fileName, FILE_READ);
    bool exists = f;
    bool isDir = f.isDirectory();
    bool isFile = !isDir;
    f.close();
    
    Filesystem.unlock();

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

void CMyWebServer::processBacktraceRequest(AsyncWebServerRequest* request)
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
    NeohubConnection.printLatestMessages(*response);
    request->send(response);
}

size_t CMyWebServer::sendFileChunk(WebResponseContext* context, uint8_t* buffer, size_t maxLen, size_t fromPosition)
{
    // Open the file and stop if we can't
    Filesystem.lock();
    File file = Filesystem.open(context->fileName, FILE_READ);
    if (!file) {
        delete context;
        Filesystem.unlock();
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

    Filesystem.unlock();
    return bytesRead;
}

void CMyWebServer::respondWithFileContents(AsyncWebServerRequest* request, const String& fileName)
{
    String contentType = getMimeType(fileName);

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

char* protectedFiles[] = { 
    "/flash/jquery-ui.min.js.gz",
    "/flash/jquery-ui.min.js",
    "/flash/jquery-3.7.1.min.js.gz",
    "/flash/jquery-3.7.1.min.js",
    nullptr
};

bool isProtected(const String &name) {
    for (char** fnp = protectedFiles; *fnp; fnp++) {
        if (name == *fnp) return true;
    }
    return false;
}

String parentDir(const String& path) {
    int end = path.length() - 1;
    while (end > 0 && path[end] == '/') end--;
    int pos = path.lastIndexOf('/', end);
    return (pos < 0) ? "" : path.substring(0, pos);
}

void CMyWebServer::respondWithDirectory(AsyncWebServerRequest* request, const String& path)
{
    String dirPath = path;
    if (!dirPath.startsWith("/")) dirPath = "/" + dirPath;
    if (!dirPath.endsWith("/")) dirPath += "/";
    
    // Collect all files in the directory
    File dir = Filesystem.open(dirPath, FILE_READ);
    if (!dir) {
        this->respondWithError(request, 404, StringPrintf("Unable to open directory '%s'", dirPath.c_str()));
        Filesystem.unlock();
        return;
    }

    if (!dir.isDirectory()) {
        dir.close();
        this->respondWithError(request, 500, StringPrintf("'%s' is not a directory", dirPath.c_str()));
        Filesystem.unlock();
        return;
    }

    std::vector<DirectoryEntry> entries = Filesystem.dir(dirPath);
    
    bool hasDate = false;
    for (DirectoryEntry& e : entries) hasDate |= e.lastModified > 0;

    // Now print HTML
    AsyncResponseStream* response = startHttpHtmlResponse(request);
    HtmlGenerator html(response);
    html.navbar(NavbarPage::Files);
    response->println("<div class='navbar-border'></div>");
    response->println("<table class='list'><thead><tr><th>File</th><th>Size (kb)</th>");
    if (hasDate) response->println("<th>Modified</th>");
    response->println("<th></th></tr></thead><tbody>");

    // Navigation
    if (dirPath == "/sdcard/") {
        response->print("<tr>");
        response->print("<td><a href='/flash'>/flash</a></td>");
        response->print("<td></td><td></td><td'></td>");
        response->print("</tr>");
    }
    else if (dirPath == "/flash/") {
        response->print("<tr>");
        response->print("<td><a href='/sdcard'>..</a></td>");
        response->print("<td></td><td></td><td'></td>");
        response->print("</tr>");
    }
    else {
        String parent = parentDir(dirPath);
        if (parent != "") {
            response->print("<tr>");
            response->printf("<td><a href='%s'>..</a></td>", parent.c_str());
            response->printf("<td></td><td></td><td'></td>");
            response->print("</tr>");
        }
    }

    Esp32CoreDump coreDump;
    if (coreDump.exists() && coreDump.getFormat() == "elf" && dirPath == "/sdcard/") {
        response->print("<tr>");
        response->printf("<td><a href='/coredump.elf'>coredump.elf</a></td>");
        response->printf("<td class='right'>%.1f</td>", coreDump.size() / 1024.0);
        response->printf("<td></td><td class='delete-file'></td></tr>");
        response->print("<tr>");
        response->printf("<td><a href='/backtrace.txt'>backtrace.txt</a></td><td></td><td></td><td></td>");
        response->printf("</tr>");
    }
    for (DirectoryEntry& e : entries) {
        response->print("<tr>");
        response->printf("<td><a href='%s'>%s</a></td>", e.path.c_str(), e.name.c_str());
        if (!e.isDirectory) {
            response->printf("<td class='right'>%.1f</td>", e.sizeKb);
        }
        else {
            response->println("<td></td>");
        }
        if (hasDate) response->printf("<td>%s</td>", e.lastModifiedText().c_str());
        if (!isProtected(e.path)) {
            if (!e.isDirectory) {
                response->println("<td class='delete-file'></td>");
            }
            else {
                response->println("<td></td>");
            }
        }
        response->println("</tr>");
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

    String path = p->value();

    // Process special file "coredump.*"
    if (path == "/coredump.elf") {
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
        if (!path.startsWith("/")) path = "/" + path; // ensure leading slash

        if (path.indexOf("..") != -1) {  // prevent navigating up
            request->send(400, "text/plain", "relative navigation not permitted");
            return;
        }

        bool result = false;

        Filesystem.lock();
        result = Filesystem.remove(path);
        Filesystem.unlock();
        
        if (result)
            request->send(200, "text/plain", "file deleted");
        else
            request->send(500, "text/plain", "delete failed");
    }

}
