#include "cache.hpp"
#include <ctime>

Cache::Cache(){
    expireTime = std::chrono::system_clock::now();
    mustRevalidate = false;
    eTag = "";
    response = Response();
}

Cache::Cache(std::chrono::system_clock::time_point expireTime, bool mustRevalidate,
             std::string eTag, Response response)
    : expireTime(expireTime), mustRevalidate(mustRevalidate), eTag(std::move(eTag)), response(std::move(response)) {}

bool Cache::isExpired() const{
    return std::chrono::system_clock::now() > expireTime;
}

std::string Cache::getExpireTimeString() const{
    std::time_t expireTime_t = std::chrono::system_clock::to_time_t(expireTime);
    std::string expireTimeStr = std::ctime(&expireTime_t);
    if(!expireTimeStr.empty() && expireTimeStr.back() == '\n'){
        expireTimeStr.pop_back();
    }
    return expireTimeStr;
}
