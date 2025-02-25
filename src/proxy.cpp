#include "proxy.hpp";
#include "cacheStore.hpp"
#include <unistd.h>



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
        //cache hit
        //in cache, valid
        //logger
        std::string responseStr = responseToString(cacheResponse);
        //log response

        return responseStr;


    }

    else if(status == CacheStatus::EXPIRED){
        //cache hit
        //logger
        //ID: cached, but expired at EXPIRE_TIME

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

        
    }

    else{
        //cache miss
        //logger
        //ID: not in cache

    }

    std::string revalidateHeader = "";
    if(mustRevalidate){
        if(!eTag.empty()){
            revalidateHeader += "If-None-Match: " + eTag + "\r\n";
        }
    }


    //logger
    //request to host

    std::string responseStr;
    Response response;
    try{
        responseStr = forwardToHost(req, requestId, clientIp, revalidateHeader);
        response = parseResponse(responseStr);
    }
    catch(...){
        //logger
        return "HTTP/1.1 502 Bad Gateway\r\n\r\n";
    }


    //logger
    //host received;



    




}



std::string Proxy::forwardToHost(const Request &req, int requestId, const std::string &clientIp, const std::string &revalidateHeader){
    std::string host = "";
    int port = 80;

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

    int fd = connectToHost(host, std::to_string(port));
    if(fd < 0){
        throw std::runtime_error("connectToHost failed");
    }

    std::string requestStr = requestToString(req, revalidateHeader);
    send(fd, requestStr.c_str(), requestStr.size(), 0);

    std::string responseStr = "";
    std::vector<char> buffer(65536);

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