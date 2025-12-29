#include "utils.hpp"
#include <netdb.h>
#include <iostream>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <vector>
#include <iomanip>
#include <ctime>
#include <algorithm>

// -----------------------------
// Small helpers
// -----------------------------
static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return (char)std::tolower(c); });
    return s;
}

// Robust send: keep sending until all bytes are written.
bool sendAll(int fd, const char *data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = send(fd, data + sent, len - sent, 0);
        if (n <= 0) return false;
        sent += (size_t)n;
    }
    return true;
}

// Read exactly len bytes (blocking). Throw if connection closes early.
static std::string recvExactOrThrow(int fd, size_t len) {
    std::string out;
    out.resize(len);
    size_t got = 0;
    while (got < len) {
        ssize_t n = recv(fd, &out[got], len - got, 0);
        if (n <= 0) {
            throw std::runtime_error("Socket closed before receiving enough bytes");
        }
        got += (size_t)n;
    }
    return out;
}

// Read until delimiter appears. Throw if exceeds maxBytes or socket closes too early.
static std::string recvUntilOrThrow(int fd, const std::string &delim, size_t maxBytes = 1024 * 1024) {
    std::string buf;
    buf.reserve(8192);
    std::vector<char> tmp(8192);

    while (buf.size() < maxBytes) {
        if (buf.find(delim) != std::string::npos) return buf;
        ssize_t n = recv(fd, tmp.data(), tmp.size(), 0);
        if (n <= 0) break;
        buf.append(tmp.data(), (size_t)n);
    }
    throw std::runtime_error("Failed to read until delimiter");
}

// Parse headers only from a header block (first line included).
static std::map<std::string, std::string> parseHeaderFieldsOnly(const std::string &headerBlock) {
    std::map<std::string, std::string> headers;
    std::istringstream iss(headerBlock);
    std::string line;

    // Skip request/status line
    std::getline(iss, line);

    while (std::getline(iss, line)) {
        if (line == "\r" || line.empty()) break;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t p = line.find(':');
        if (p == std::string::npos) continue;
        std::string k = line.substr(0, p);
        std::string v = line.substr(p + 1);
        if (!v.empty() && v.front() == ' ') v.erase(0, 1);
        headers[k] = v;
    }
    return headers;
}

// -----------------------------
// connectToOther
// -----------------------------
int connectToOther(const std::string &ip, const std::string &portStr){
    struct addrinfo hints, *serverInfo;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(ip.c_str(), portStr.c_str(), &hints, &serverInfo);
    if(status != 0){
        std::cerr << "getaddrinfo error: " << gai_strerror(status) << std::endl;
        return -1;
    }

    struct addrinfo *p;
    int socketFd = -1;
    for(p = serverInfo; p != nullptr; p = p->ai_next){
        socketFd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if(socketFd < 0) continue;

        if(connect(socketFd, p->ai_addr, p->ai_addrlen) < 0){
            close(socketFd);
            socketFd = -1;
            continue;
        }
        break;
    }
    freeaddrinfo(serverInfo);
    return socketFd;
}

// -----------------------------
// parseRequest / parseResponse (FIXED)
// Require full body bytes when Content-Length exists.
// -----------------------------
Request parseRequest(const std::string &request){
    std::istringstream iss(request);
    std::string line;
    Request req;

    if(!std::getline(iss, line)){
        throw std::runtime_error("Invalid request (no start line)");
    }
    if(!line.empty() && line.back() == '\r') line.pop_back();

    std::istringstream firstLine(line);
    firstLine >> req.method >> req.url >> req.version;

    // Headers
    while(std::getline(iss, line) && line != "\r" && !line.empty()){
        if(!line.empty() && line.back() == '\r') line.pop_back();
        size_t col_pos = line.find(":");
        if(col_pos == std::string::npos){
            throw std::runtime_error("Invalid header: " + line);
        }
        std::string key = line.substr(0, col_pos);
        std::string value = line.substr(col_pos + 1);
        if(!value.empty() && value.front() == ' ') value.erase(0,1);
        req.headers[key] = value;
    }

    // Body (Content-Length only)
    auto it = req.headers.find("Content-Length");
    if(it != req.headers.end()){
        int len = 0;
        try {
            len = std::stoi(it->second);
        } catch (...) {
            throw std::runtime_error("Invalid Content-Length");
        }
        if (len < 0) throw std::runtime_error("Negative Content-Length");

        std::string body(len, '\0');
        iss.read(&body[0], len);

        // Enforce full read
        if (iss.gcount() != (std::streamsize)len) {
            throw std::runtime_error("Incomplete request body for Content-Length");
        }
        req.body = body;
    } else {
        req.body = "";
    }
    return req;
}

Response parseResponse(const std::string &response){
    std::istringstream iss(response);
    std::string line;
    Response res;

    if(!std::getline(iss, line)){
        throw std::runtime_error("Invalid response (no status line)");
    }
    if(!line.empty() && line.back() == '\r') line.pop_back();

    std::istringstream statusLine(line);
    statusLine >> res.version >> res.status_code;

    std::getline(statusLine, res.status_msg);
    if(!res.status_msg.empty() && res.status_msg.front() == ' ')
        res.status_msg.erase(0, res.status_msg.find_first_not_of(' '));

    // Headers
    while(std::getline(iss, line) && line != "\r" && !line.empty()){
        if(!line.empty() && line.back() == '\r') line.pop_back();
        size_t col_pos = line.find(":");
        if(col_pos == std::string::npos){
            throw std::runtime_error("Invalid header: " + line);
        }
        std::string key = line.substr(0, col_pos);
        std::string value = line.substr(col_pos + 1);
        if(!value.empty() && value.front() == ' ') value.erase(0,1);
        res.headers[key] = value;
    }

    // Body: chunked
    auto te = res.headers.find("Transfer-Encoding");
    if (te != res.headers.end() && toLower(te->second) == "chunked") {
        std::string chunkedBody;
        char ch;
        while (iss.get(ch)) chunkedBody += ch;
        res.body = handleChunk(chunkedBody);
        return res;
    }

    // Body: Content-Length
    auto cl = res.headers.find("Content-Length");
    if (cl != res.headers.end()) {
        int len = 0;
        try {
            len = std::stoi(cl->second);
        } catch (...) {
            throw std::runtime_error("Invalid Content-Length");
        }
        if (len < 0) throw std::runtime_error("Negative Content-Length");

        std::string body(len, '\0');
        iss.read(&body[0], len);

        // Enforce full read
        if (iss.gcount() != (std::streamsize)len) {
            throw std::runtime_error("Incomplete response body for Content-Length");
        }
        res.body = body;
    } else {
        res.body = "";
    }

    return res;
}

// -----------------------------
// Chunk decoding
// -----------------------------
std::string handleChunk(const std::string &chunkedBody) {
    std::istringstream stream(chunkedBody);
    std::string final_string;
    std::string line;

    while (std::getline(stream, line)) {
        if(line.empty()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        size_t chunkSize = 0;
        std::istringstream sizeStream(line);
        sizeStream >> std::hex >> chunkSize;
        if (chunkSize == 0) {
            break;
        }

        std::vector<char> buffer(chunkSize);
        stream.read(buffer.data(), (std::streamsize)chunkSize);

        if (stream.gcount() != (std::streamsize)chunkSize) {
            throw std::runtime_error("Chunk data size mismatch");
        }

        final_string.append(buffer.data(), chunkSize);

        // Consume CRLF after chunk data
        if (!std::getline(stream, line)) {
            throw std::runtime_error("Missing CRLF after chunk");
        }
    }

    return final_string;
}

// -----------------------------
// HTTP date parser (GMT/UTC)
// -----------------------------
std::chrono::system_clock::time_point parseHttpDateToTimePoint(const std::string &httpDate) {
    std::tm tm = {};
    std::istringstream iss(httpDate);

    // Example: "Sun, 06 Nov 1994 08:49:37 GMT"
    iss >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S");
    if (iss.fail()) {
        throw std::runtime_error("Failed to parse HTTP date");
    }

    // timegm converts tm (UTC) -> time_t
    time_t tt = timegm(&tm);
    if (tt == (time_t)-1) {
        throw std::runtime_error("Failed to convert parsed time to UTC");
    }
    return std::chrono::system_clock::from_time_t(tt);
}

// -----------------------------
// Wire serializers
// -----------------------------
std::string requestToString(const Request &request, const std::string &extraHeaderBlock){
    std::ostringstream oss;
    oss << request.method << " " << request.url << " " << request.version << "\r\n";
    for(const auto &h: request.headers){
        oss << h.first << ": " << h.second << "\r\n";
    }
    // extraHeaderBlock must contain complete lines ending with \r\n
    if(!extraHeaderBlock.empty()){
        oss << extraHeaderBlock;
    }
    oss << "\r\n";
    if(!request.body.empty()){
        oss.write(request.body.data(), (std::streamsize)request.body.size());
    }
    return oss.str();
}

// Serialize response for client: do not truncate; keep binary safe.
// If response was chunked and already decoded, convert to Content-Length.
std::string serializeResponseForClient(Response response){
    auto te = response.headers.find("Transfer-Encoding");
    if (te != response.headers.end() && toLower(te->second) == "chunked") {
        response.headers.erase("Transfer-Encoding");
        response.headers["Content-Length"] = std::to_string(response.body.size());
    }

    std::ostringstream oss;
    oss << response.version << " " << response.status_code << " " << response.status_msg << "\r\n";
    for(const auto &h: response.headers){
        oss << h.first << ": " << h.second << "\r\n";
    }
    oss << "\r\n";
    if(!response.body.empty()){
        oss.write(response.body.data(), (std::streamsize)response.body.size());
    }
    return oss.str();
}

// Log string only: may truncate body to avoid huge logs.
std::string responseToLogString(const Response &response){
    std::ostringstream oss;
    oss << response.version << " " << response.status_code << " " << response.status_msg << "\r\n";
    for(const auto &h: response.headers){
        oss << h.first << ": " << h.second << "\r\n";
    }
    oss << "\r\n";

    const size_t kMax = 500;
    size_t n = std::min(kMax, response.body.size());
    if (n > 0) oss.write(response.body.data(), (std::streamsize)n);
    if (response.body.size() > n) oss << "...";
    return oss.str();
}

// -----------------------------
// Socket framing readers (FIXED)
// Do not rely on "server closes connection" for HTTP/1.1.
// -----------------------------
Request readHttpRequestFromFd(int clientFd){
    // Read headers fully first
    std::string raw = recvUntilOrThrow(clientFd, "\r\n\r\n");
    size_t headerEnd = raw.find("\r\n\r\n");
    std::string headerPart = raw.substr(0, headerEnd + 4);

    // Determine Content-Length using header-only parse
    auto headers = parseHeaderFieldsOnly(headerPart);
    size_t needBody = 0;
    auto cl = headers.find("Content-Length");
    if (cl != headers.end()) {
        int len = std::stoi(cl->second);
        if (len < 0) throw std::runtime_error("Negative Content-Length");
        needBody = (size_t)len;
    }

    // Keep any already received bytes after header
    std::string bodyAlready = raw.substr(headerEnd + 4);

    if (bodyAlready.size() < needBody) {
        bodyAlready += recvExactOrThrow(clientFd, needBody - bodyAlready.size());
    }

    std::string full = headerPart + bodyAlready.substr(0, needBody);
    return parseRequest(full);
}

Response readHttpResponseFromFd(int serverFd){
    // Read headers fully first
    std::string raw = recvUntilOrThrow(serverFd, "\r\n\r\n");
    size_t headerEnd = raw.find("\r\n\r\n");
    std::string headerPart = raw.substr(0, headerEnd + 4);

    auto headers = parseHeaderFieldsOnly(headerPart);

    // Keep any already received bytes after header
    std::string bodyAlready = raw.substr(headerEnd + 4);

    // Case 1: chunked
    auto te = headers.find("Transfer-Encoding");
    if (te != headers.end() && toLower(te->second) == "chunked") {
        // Simplified framing: read until the terminating chunk marker appears.
        std::string chunked = bodyAlready;
        while (chunked.find("\r\n0\r\n\r\n") == std::string::npos) {
            std::vector<char> tmp(8192);
            ssize_t n = recv(serverFd, tmp.data(), tmp.size(), 0);
            if (n <= 0) break;
            chunked.append(tmp.data(), (size_t)n);
            if (chunked.size() > 50 * 1024 * 1024) throw std::runtime_error("Response too large");
        }
        return parseResponse(headerPart + chunked);
    }

    // Case 2: Content-Length
    auto cl = headers.find("Content-Length");
    if (cl != headers.end()) {
        int len = std::stoi(cl->second);
        if (len < 0) throw std::runtime_error("Negative Content-Length");
        size_t needBody = (size_t)len;

        if (bodyAlready.size() < needBody) {
            bodyAlready += recvExactOrThrow(serverFd, needBody - bodyAlready.size());
        }

        std::string full = headerPart + bodyAlready.substr(0, needBody);
        return parseResponse(full);
    }

    // Case 3: fallback (Connection: close)
    std::string tail = bodyAlready;
    std::vector<char> tmp(8192);
    while (true) {
        ssize_t n = recv(serverFd, tmp.data(), tmp.size(), 0);
        if (n <= 0) break;
        tail.append(tmp.data(), (size_t)n);
        if (tail.size() > 50 * 1024 * 1024) throw std::runtime_error("Response too large");
    }
    return parseResponse(headerPart + tail);
}
