#include "proxy.hpp"
#include "cacheStore.hpp"
#include <unistd.h>
#include <sstream>
#include "logger.hpp"
#include <vector>





/// Client sends request to proxy
//Proxy checks CacheStore
    //If cache hit, return response
    //If cache miss, forward request to host
//Host sends response to proxy
//304 Not Modified Use the cached response.
//200 OK Store the new response in the cache.
//Proxy responds to the Client




Proxy::Proxy() {}
Proxy::~Proxy() {}

std::string Proxy::handleGet(const Request &req, int requestId, const std::string &clientIp){
    
    Response cacheResponse;
    std::string key = req.url;
    std::string expireTimeStr;
    std::string eTag;
    bool mustRevalidate = false;

    CacheStatus status = CacheStore::getInstance().fetchData(key, cacheResponse, expireTimeStr);


    if(status == CacheStatus::VALID){
        //in cache, valid
        //logger
        Logger::getInstance().logCacheStatus(requestId, "in cache, valid");
        std::string responseStr = responseToString(cacheResponse);
        //log response
        //HTTP/1.1 200 OK


        return responseStr;


    }

    else if(status == CacheStatus::EXPIRED){
        //cache hit
        //logger
        //ID: cached, but expired at EXPIRE_TIME
        Logger::getInstance().logCacheStatusExpired(requestId, expireTimeStr);

        if(cacheResponse.headers.find("ETag") != cacheResponse.headers.end()){
            eTag = cacheResponse.headers["ETag"];
        }
        else{
            eTag = "";
        }

        mustRevalidate = true;
    }

    else if(status == CacheStatus::REVALIDATE){
        //cache hit
        //logger
        //in cache, but requires re-validation
        Logger::getInstance().logCacheStatus(requestId, "in cache, requires validation");
        
    }

    else{
        //cache miss
        //logger
        //ID: not in cache
        Logger::getInstance().logCacheStatus(requestId, "not in cache");
    }

    std::string revalidateHeader = "";
    if(mustRevalidate){
        if(!eTag.empty()){
            revalidateHeader += "If-None-Match: " + eTag + "\r\n";
        }
    }


    //logger
    //request to host
    // 104: Requesting "GET /index.html HTTP/1.1" from www.example.com
    {std::ostringstream line;
    
    line << req.method << " " << req.url << " " << req.version;
    std::string host = "unknown";
    if(req.headers.find("Host")!=req.headers.end()){
        host = req.headers.at("Host");
    }
    Logger::getInstance().logRequesting(requestId, line.str(), host);
    }



    std::string responseStr;
    Response response;
    //strong exception safety guarantee
    //If parsing fails, we catch the exception and return an error without modifying the proxy state.
    //Remains in a consistent state as if the request never happened.
    try{
        responseStr = forwardToHost(req, requestId, clientIp, revalidateHeader);
        response = parseResponse(responseStr);
    }
    catch(...){
        //logger
        Logger::getInstance().logRespond(requestId, "HTTP/1.1 502 Bad Gateway");
        return "HTTP/1.1 502 Bad Gateway\r\n\r\n";
    }


    //logger
    //host received;
    {std::ostringstream line;
    line << response.version << " " << response.status_code << " " << response.status_msg;
    std::string host = "unknown";
    if(req.headers.find("Host")!=req.headers.end()){
        host = req.headers.at("Host");
    }
    Logger::getInstance().logReceived(requestId, responseToString(response), host);
    }



    if(response.status_code == 304 && mustRevalidate){

        CacheStore::getInstance().storeData(key, response);
        std::string finalStr = responseToString(cacheResponse);
        //logger
        //304 NotModified->Using Old
        std::ostringstream line;
        line << cacheResponse.version << " (304 Not Modified -> Using Old)";
        Logger::getInstance().logRespond(requestId, line.str());
        return finalStr;
    }


    //200 ok store new response in cache
    else if(response.status_code == 200){


        std::string reason = "";
        bool canStore = true;
        if(response.headers.find("Cache-Control") != response.headers.end()){
            auto cacheControl = response.headers.find("Cache-Control");
            if(cacheControl->second.find("no-store") != std::string::npos){
                canStore = false;
                reason="Cache-Control: no-store";
                
            }
            else if(cacheControl->second.find("private") != std::string::npos){
                canStore = false;
                reason="Cache-Control: private";
                
            }
            
        }


        if(canStore){
            CacheStore::getInstance().storeData(key, response);
            
            Response temp;
            std::string expTime;
            CacheStatus tempStatus = CacheStore::getInstance().fetchData(key, temp, expTime);
            if(tempStatus == CacheStatus::REVALIDATE){
                //logger
                //ID: cached, but requires re-validation
                Logger::getInstance().logCachedButRevalidate(requestId);
            }
            else{
                //logger
                //ID: cached, expires at EXPIRES
                Logger::getInstance().logCachedExpires(requestId, expTime);
            }
        }
        else{
            //logger
            //logNotCacheable
            Logger::getInstance().logNotCacheable(requestId, reason);
        }



        std::string responseStr = responseToString(response);


        std::ostringstream oss;
        oss << response.version << " " << response.status_code << " " << response.status_msg;
        //logger
        //Responding HTTP/1.1 200 OK
        std::ostringstream line;
        line << response.version << " " << response.status_code << " " << response.status_msg;
        Logger::getInstance().logRespond(requestId, line.str());
        return responseStr;
    }

    else if(response.status_code == 404){
        Logger::getInstance().logRespond(requestId, "HTTP/1.1 404 Not Found");
        return responseStr;
    }

    else{
        Logger::getInstance().logRespond(requestId, responseStr);
        return responseStr;
    }


}

std::string Proxy::handlePost(const Request &req, int requestId, const std::string &clientIp){

    std::string hostName = "unknown";
    if(req.headers.find("Host") != req.headers.end()){
        hostName = req.headers.at("Host");
    }


    {std::ostringstream oss;
    oss << req.method << " " << req.url << " " << req.version << "\r\n";
    
    //logger
    //eg 105: Requesting "POST /login HTTP/1.1" from example.com
    Logger::getInstance().logRequesting(requestId, oss.str(), hostName);
    }



    std::string responseStr;
    Response response;
    //strong guarantee
    //The execution of forwardToHost operation either successfully returns a complete response string
    //or if any exception occurs, it logs the error, and returns a standard "HTTP/1.1 502 Bad Gateway" response. 
    ////Remains in a consistent state as if the request never happened.
    try{
        responseStr = forwardToHost(req, requestId, clientIp, "");
    }
    catch(...){
        //logger
        Logger::getInstance().logRespond(requestId, "HTTP/1.1 502 Bad Gateway");
        return "HTTP/1.1 502 Bad Gateway\r\n\r\n";
    }

    //strong exception safety guarantee
    //If parsing fails, we catch the exception and return an error without modifying the proxy state.
    //Remains in a consistent state as if the request never happened.
    try{
        response = parseResponse(responseStr);
    }
    catch(...){
        //logger
        Logger::getInstance().logRespond(requestId, "HTTP/1.1 502 Bad Gateway");
        return "HTTP/1.1 502 Bad Gateway\r\n\r\n";
    }



    
    //logger
    //105: Received "HTTP/1.1 201 Created" from api.example.com
    

    responseStr = responseToString(response);
    {std::ostringstream oss;
    oss << response.version << " " << response.status_code << " " << response.status_msg;
    Logger::getInstance().logReceived(requestId, responseStr, hostName);
    }

    //logger
    //105: Responding "HTTP/1.1 201 Created
    std::ostringstream line;
    line << response.version << " " << response.status_code << " " << response.status_msg;
    Logger::getInstance().logRespond(requestId, line.str());




    return responseStr;


 }


 std::string Proxy::handleConnect(const Request &req, int requestId, const std::string &clientIp){
    // 建議把 log 也一併改掉，避免誤解
    Logger::getInstance().logRespond(requestId, "HTTP/1.1 200 Connection Established");

    // 瀏覽器預期看到 "Connection Established" 而不是 "OK"
    return "HTTP/1.1 200 Connection Established\r\n"
           "Proxy-Agent: MyProxy/1.0\r\n"
           "\r\n";
}


 



std::string Proxy::forwardToHost(const Request &req, int requestId, const std::string &clientIp, const std::string &revalidateHeader){
    std::string host = "";
    int port = 80;
    //Locally implements Strong Exception Guarantee because if the conversion fails, it does not modify the originally expected state.
    if(req.headers.find("Host") != req.headers.end()){
        host = req.headers.at("Host");
        size_t colonPos = host.find(":");
        if(colonPos != std::string::npos){
            try{
                port = std::stoi(host.substr(colonPos + 1));
            }catch(...){
                port = 80;
            }
            host = host.substr(0, colonPos);
        }
    }
    //The operation either succeeds completely  or does not affect the internal state of the Proxy at all 
    //strong Guarantee
    int fd = connectToHost(host, std::to_string(port));
    if(fd < 0){
        Logger::getInstance().logError(0, "Failed to connect to host " + host + ":" + std::to_string(port));
        throw std::runtime_error("connectToHost failed");
    }

    std::string requestStr = requestToString(req, revalidateHeader);
    send(fd, requestStr.c_str(), requestStr.size(), 0);

    std::string responseStr = "";
    std::vector<char> buffer(65536);
    //basic guarantee
    while(true){
        int numBytes = recv(fd, buffer.data(), buffer.size() - 1, 0);
        if(numBytes <= 0){
            break;
        }
        responseStr += std::string(buffer.data(), numBytes);
    }
    close(fd); 
    return responseStr;


}





int Proxy::connectToHost(const std::string &host, const std::string &portStr){
    return connectToOther(host, portStr);
}





