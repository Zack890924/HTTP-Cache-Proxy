#include "cacheStore.hpp"
#include "cache.hpp"
#include <iostream>







bool CacheStore::fetchData(const std::string &key, Response &result) {
	std::shared_lock<std::shared_mutex> lock(cacheMutex);
    auto it = cacheStore.find(key);
    if(it == cacheStore.end()){
        //not found
        return false;
    }
    const Cache &cache = it->second;
    
    if(cache.isExpired()){
        //expired
        return false;
    }

    if(cache.mustRevalidate){
        return false;
    }

    result = cache.response;
    return true;
}


void CacheStore::storeData(const std::string &key, const Response response) {

    if(response.status_code != 200){
        return;
    }

    auto it = response.headers.find("Cache-Control");
    auto expireTime = std::chrono::system_clock::now();
    bool revalidate = false;

    if(it != response.headers.end()){
        std::string cacheControl = it->second;
        if(cacheControl.find("no-store") != std::string::npos){
            return;
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

        if(cacheControl.find("must-revalidate") != std::string::npos){
            revalidate = true;
        }
    }
    std::string eTagVal = "";
    auto eTag = response.headers.find("ETag");
    if(eTag != response.headers.end()){
        eTagVal = eTag->second;
    }


    Cache cache(expireTime, revalidate, eTagVal, response);

    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    cacheStore[key] = cache;
    
}

