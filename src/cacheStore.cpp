#include "cacheStore.hpp"
#include "cache.hpp"
#include <iostream>
#include <sstream>
#include "logger.hpp"
#include "utils.hpp"


static const size_t DEFAULT_SIZE = 10;

CacheStore::CacheStore() {
    MAX_CACHE_SIZE = DEFAULT_SIZE; 
    //This constructor does not throw exceptions because it only initializes members.
}






void CacheStore::moveToFront(const std::string &key) {
    auto it = cacheMap.find(key);
    if (it != cacheMap.end()) {
        dLlist.erase(it->second);
    }
    dLlist.push_front(key);
    cacheMap[key] = dLlist.begin();
}



std::pair<std::chrono::system_clock::time_point, bool> CacheStore::parseCacheControl(const std::map<std::string, std::string>& headers){
    auto expireTime = std::chrono::system_clock::now() + std::chrono::seconds(60);
    bool revalidate = false;

    auto controlIt = headers.find("Cache-Control");
    if(controlIt != headers.end()){
        std::string cacheControl = controlIt->second;


        if(cacheControl.find("no-store") != std::string::npos || cacheControl.find("private") != std::string::npos) {
            throw std::runtime_error("Not cache");
        }
        if(cacheControl.find("must-revalidate") != std::string::npos){
            revalidate = true;
        }
        
        auto startPos = cacheControl.find("max-age=");
        if(startPos != std::string::npos){
            startPos += 8; 
            auto endPos = cacheControl.find(", ", startPos);
            if(endPos == std::string::npos){
                endPos = cacheControl.size();
            }
            
            std::string maxAgeStr;
            for(auto i = startPos; i < endPos; i++){
                if(isdigit(cacheControl[i])){
                    maxAgeStr += cacheControl[i];
                } else {
                    break;
                }
            }
            if(!maxAgeStr.empty()){
                int maxAge = std::stoi(maxAgeStr);
                expireTime = std::chrono::system_clock::now() + std::chrono::seconds(maxAge);
            }
        }
    }
        //strong guarantee
        //If an exception occurs during the update,log the error and throw, ensuring the state remains unchanged
        auto expiresIt = headers.find("Expires");
        if(expiresIt != headers.end()){
            try {
                auto expTimeParsed = parseHttpDateToTimePoint(expiresIt->second);
                if(expTimeParsed > std::chrono::system_clock::now()){
                    expireTime = expTimeParsed;
                }
            } catch (const std::exception &e) {
                Logger::getInstance().logError(0, "Failed to parse Expires: " + std::string(e.what()));
            }
        }
        
        return {expireTime, revalidate};
    }

    

CacheStatus CacheStore::fetchData(const std::string &key, Response &result, std::string &expireTimeStr) {
	std::shared_lock<std::shared_mutex> lock(cacheMutex);
    auto it = data.find(key);
    if(it == data.end()){
        //not found
        return CacheStatus::MISS;
    }

    moveToFront(key);

    const Cache &cache = it->second;
    
    if(cache.isExpired()){
        //expired
        return CacheStatus::EXPIRED;
    }

    if(cache.mustRevalidate){
        return CacheStatus::REVALIDATE;
    }

    result = cache.response;
    expireTimeStr = cache.getExpireTimeString();
    return CacheStatus::VALID;
}



void CacheStore::storeData(const std::string &key, Response response) {

    if(response.status_code != 200){
        return;
    }
    
    
    //strong guarantee
    //If an exception occurs during the update,log the error and throw, ensuring the state remains unchanged
    std::chrono::system_clock::time_point expireTime;
    bool revalidate = false;
    try {
        std::tie(expireTime, revalidate) = parseCacheControl(response.headers);
    } catch (const std::runtime_error &e) {
        Logger::getInstance().logError(0, "Cache skipped: " + std::string(e.what()));
        return;
    }

        
    
    std::string eTagVal = "";
    auto eTag = response.headers.find("ETag");
    if(eTag != response.headers.end()){
        eTagVal = eTag->second;
    }


    Cache cache(expireTime, revalidate, eTagVal, response);

    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    //strong guarantee
    //If an exception occurs during the update,log the error and throw, ensuring the state remains unchanged
    //If an error happend, operator leaves the system state as if it never happened.
    try {
        auto it = data.find(key);
        if (it == data.end()) {
            dLlist.push_front(key);
            cacheMap[key] = dLlist.begin();
            data[key] = cache;
        } else {
            it->second = cache;
            moveToFront(key);
        }
    } catch (const std::exception &e) {
        Logger::getInstance().logError(0, "Cache update failed: " + std::string(e.what()));
        throw;
    }


    if (data.size() > MAX_CACHE_SIZE) {
        std::string oldKey = dLlist.back(); 
        dLlist.pop_back();  
        data.erase(oldKey); 
        cacheMap.erase(oldKey);  
    }
    
    
}


void CacheStore::updateCacheHeaders(const std::string &key, const Response &newResponse) {
    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    auto it = data.find(key);
    if (it != data.end()) {
        Cache &cached = it->second;

        for (const auto &kv : newResponse.headers) {
            cached.response.headers[kv.first] = kv.second;
        }
 
        auto eTagIt = newResponse.headers.find("ETag");
        if (eTagIt != newResponse.headers.end()) {
            cached.eTag = eTagIt->second;
        }

        //strong guarantee
        //If an exception occurs during the update,log the error and throw, ensuring the state remains unchanged
        try {
            auto [newExpireTime, newRevalidate] = parseCacheControl(newResponse.headers);
            cached.expireTime = newExpireTime;
            cached.mustRevalidate = newRevalidate;
        } catch (const std::runtime_error &e) {
            Logger::getInstance().logError(0, "Cache header update skipped: " + std::string(e.what()));
        }
    }
}




