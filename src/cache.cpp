#include "cache.hpp";



Cache::Cache(){
    expireTime = std::chrono::system_clock::now();
    mustRevalidate = false;
    eTag = "";
    response = Response();
}


Cache::Cache(std::chrono::system_clock::time_point expireTime, bool mustRevalidate, std::string eTag, Response response) :
                 expireTime(expireTime), mustRevalidate(mustRevalidate), eTag(eTag), response(response){}




bool Cache::isExpired(){
    return std::chrono::system_clock::now() > expireTime;
}


std::string Cache::getExpireTime(){
    std::time_t expireTime_t = std::chrono::system_clock::to_time_t(expireTime);
    //time -> string
    std::string expireTimeStr = std::ctime(&expireTime_t);
    //remove \n
    expireTimeStr.pop_back();
    return expireTimeStr;
}



