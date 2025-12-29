#include "proxy.hpp"
#include "cacheStore.hpp"
#include "logger.hpp"
#include <unistd.h>
#include <sstream>

Proxy::Proxy() {}
Proxy::~Proxy() {}

std::string Proxy::makeCacheKey(const Request &req) {
    std::string host = "unknown";
    auto it = req.headers.find("Host");
    if (it != req.headers.end()) host = it->second;
    return host + "|" + req.url;
}

int Proxy::connectToHost(const std::string &host, const std::string &portStr){
    return connectToOther(host, portStr);
}

Response Proxy::forwardToHostResponse(const Request &req, int requestId, const std::string &extraHeaderBlock){
    std::string host;
    int port = 80;

    auto it = req.headers.find("Host");
    if(it != req.headers.end()){
        host = it->second;
        size_t colonPos = host.find(":");
        if(colonPos != std::string::npos){
            try { port = std::stoi(host.substr(colonPos + 1)); } catch (...) { port = 80; }
            host = host.substr(0, colonPos);
        }
    }

    int fd = connectToHost(host, std::to_string(port));
    if(fd < 0){
        Logger::getInstance().logError(requestId, "Failed to connect to host " + host);
        throw std::runtime_error("connectToHost failed");
    }

    std::string reqBytes = requestToString(req, extraHeaderBlock);
    if(!sendAll(fd, reqBytes.data(), reqBytes.size())){
        close(fd);
        throw std::runtime_error("sendAll failed");
    }

    // Read response using proper framing (CL/chunked/close)
    Response res = readHttpResponseFromFd(fd);
    close(fd);
    return res;
}

std::string Proxy::handleGet(const Request &req, int requestId, const std::string &clientIp){
    (void)clientIp;

    Response cacheResponse;
    std::string expireTimeStr;

    std::string key = makeCacheKey(req);

    CacheStatus status = CacheStore::getInstance().fetchData(key, cacheResponse, expireTimeStr);

    if(status == CacheStatus::VALID){
        Logger::getInstance().logCacheStatus(requestId, "in cache, valid");
        return serializeResponseForClient(cacheResponse);
    }

    bool needConditional = (status == CacheStatus::EXPIRED || status == CacheStatus::REVALIDATE);

    if(status == CacheStatus::EXPIRED){
        Logger::getInstance().logCacheStatusExpired(requestId, expireTimeStr);
    } else if(status == CacheStatus::REVALIDATE){
        Logger::getInstance().logCacheStatus(requestId, "in cache, requires validation");
    } else {
        Logger::getInstance().logCacheStatus(requestId, "not in cache");
    }

    std::string extraHeaders;
    if(needConditional){
        auto it = cacheResponse.headers.find("ETag");
        if(it != cacheResponse.headers.end()){
            extraHeaders += "If-None-Match: " + it->second + "\r\n";
        }
    }

    // Log outgoing request
    {
        std::ostringstream line;
        line << req.method << " " << req.url << " " << req.version;
        std::string hostName = "unknown";
        auto hit = req.headers.find("Host");
        if(hit != req.headers.end()) hostName = hit->second;
        Logger::getInstance().logRequesting(requestId, line.str(), hostName);
    }

    Response response;
    try{
        response = forwardToHostResponse(req, requestId, extraHeaders);
    } catch (...) {
        Logger::getInstance().logRespond(requestId, "HTTP/1.1 502 Bad Gateway");
        return "HTTP/1.1 502 Bad Gateway\r\n\r\n";
    }

    // Log received response (safe truncation)
    {
        std::string hostName = "unknown";
        auto hit = req.headers.find("Host");
        if(hit != req.headers.end()) hostName = hit->second;
        Logger::getInstance().logReceived(requestId, responseToLogString(response), hostName);
    }

    // 304: reuse cached body, update cached headers
    if(response.status_code == 304 && needConditional){
        CacheStore::getInstance().updateCacheHeaders(key, response);

        Response updated;
        std::string tmp;
        CacheStore::getInstance().fetchData(key, updated, tmp);

        Logger::getInstance().logRespond(requestId, updated.version + " (304 Not Modified -> Using Old)");
        return serializeResponseForClient(updated);
    }

    // 200: cache if allowed
    if(response.status_code == 200){
        bool canStore = true;
        std::string reason;

        auto cc = response.headers.find("Cache-Control");
        if(cc != response.headers.end()){
            if(cc->second.find("no-store") != std::string::npos){
                canStore = false; reason = "Cache-Control: no-store";
            } else if(cc->second.find("private") != std::string::npos){
                canStore = false; reason = "Cache-Control: private";
            }
        }

        if(canStore){
            CacheStore::getInstance().storeData(key, response);

            Response tmpResp;
            std::string exp;
            CacheStatus st = CacheStore::getInstance().fetchData(key, tmpResp, exp);
            if(st == CacheStatus::REVALIDATE) Logger::getInstance().logCachedButRevalidate(requestId);
            else Logger::getInstance().logCachedExpires(requestId, exp);
        } else {
            Logger::getInstance().logNotCacheable(requestId, reason);
        }

        std::ostringstream line;
        line << response.version << " " << response.status_code << " " << response.status_msg;
        Logger::getInstance().logRespond(requestId, line.str());
        return serializeResponseForClient(response);
    }

    // Other status: pass-through
    {
        std::ostringstream line;
        line << response.version << " " << response.status_code << " " << response.status_msg;
        Logger::getInstance().logRespond(requestId, line.str());
    }
    return serializeResponseForClient(response);
}

std::string Proxy::handlePost(const Request &req, int requestId, const std::string &clientIp){
    (void)clientIp;

    std::string hostName = "unknown";
    auto it = req.headers.find("Host");
    if(it != req.headers.end()) hostName = it->second;

    // Log outgoing request
    {
        std::ostringstream oss;
        oss << req.method << " " << req.url << " " << req.version;
        Logger::getInstance().logRequesting(requestId, oss.str(), hostName);
    }

    Response response;
    try{
        response = forwardToHostResponse(req, requestId, "");
    } catch (...) {
        Logger::getInstance().logRespond(requestId, "HTTP/1.1 502 Bad Gateway");
        return "HTTP/1.1 502 Bad Gateway\r\n\r\n";
    }

    // Log received response
    Logger::getInstance().logReceived(requestId, responseToLogString(response), hostName);

    // Log responding
    {
        std::ostringstream line;
        line << response.version << " " << response.status_code << " " << response.status_msg;
        Logger::getInstance().logRespond(requestId, line.str());
    }

    return serializeResponseForClient(response);
}

std::string Proxy::handleConnect(const Request &req, int requestId, const std::string &clientIp){
    (void)req; (void)clientIp;

    Logger::getInstance().logRespond(requestId, "HTTP/1.1 200 Connection Established");

    return "HTTP/1.1 200 Connection Established\r\n"
           "Proxy-Agent: MyProxy/1.0\r\n"
           "\r\n";
}
