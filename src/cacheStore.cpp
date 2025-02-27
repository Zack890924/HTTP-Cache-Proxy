#include "cacheStore.hpp"
#include "cache.hpp"
#include <iostream>


static const size_t DEFAULT_SIZE = 10;

CacheStore::CacheStore() {
    MAX_CACHE_SIZE = DEFAULT_SIZE; 
}


void CacheStore::moveToFront(const std::string &key) {
    auto it = cacheMap.find(key);
    if (it != cacheMap.end()) {
        dLlist.erase(it->second);
    }
    dLlist.push_front(key);
    cacheMap[key] = dLlist.begin();
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
    return CacheStatus::VALID;
}



void CacheStore::storeData(const std::string &key, const Response response) {

    if(response.status_code != 200){
        return;
    }

    auto controlIt = response.headers.find("Cache-Control");
    auto expireTime = std::chrono::system_clock::now() + std::chrono::seconds(60);
    bool revalidate = false;
    

    if(controlIt != response.headers.end()){
        std::string cacheControl = controlIt->second;
        if(cacheControl.find("no-store") != std::string::npos){
            return;
        }
        if(cacheControl.find("private") != std::string::npos){
            return;
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
                }
                else{
                    std::cerr<< "Invalid max-age value" << std::endl;
                    break;
                }
            }

            if(!maxAgeStr.empty()){
                int maxAge = std::stoi(maxAgeStr);
                expireTime += std::chrono::seconds(maxAge);
            }
            else{
                expireTime += std::chrono::seconds(60);
            }

        }

        
    }
    std::string eTagVal = "";
    auto eTag = response.headers.find("ETag");
    if(eTag != response.headers.end()){
        eTagVal = eTag->second;
    }


    Cache cache(expireTime, revalidate, eTagVal, response);

    std::unique_lock<std::shared_mutex> lock(cacheMutex);

    auto it = data.find(key);
    if(it == data.end()){
        dLlist.push_front(key);
        cacheMap[key] = dLlist.begin();
        data[key] = cache;
    } 
    else{
        it->second = cache; 
        moveToFront(key);
    }


    if (data.size() > MAX_CACHE_SIZE) {
        std::string oldKey = dLlist.back(); 
        dLlist.pop_back();  
        data.erase(oldKey); 
        cacheMap.erase(oldKey);  
    }
    
    
}




