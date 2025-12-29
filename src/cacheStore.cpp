#include "cacheStore.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include <tuple>
#include <cctype>

static const size_t DEFAULT_SIZE = 10;

CacheStore::CacheStore() : MAX_CACHE_SIZE(DEFAULT_SIZE) {}

void CacheStore::moveToFrontUnsafe(const std::string &key) {
    auto it = cacheMap.find(key);
    if (it != cacheMap.end()) {
        dLlist.erase(it->second);
    }
    dLlist.push_front(key);
    cacheMap[key] = dLlist.begin();
}

std::pair<std::chrono::system_clock::time_point, bool>
CacheStore::parseCacheControl(const std::map<std::string, std::string>& headers){
    // Default: cache for 60 seconds if no cache headers exist.
    auto expireTime = std::chrono::system_clock::now() + std::chrono::seconds(60);
    bool revalidate = false;

    auto controlIt = headers.find("Cache-Control");
    if(controlIt != headers.end()){
        std::string cacheControl = controlIt->second;

        // Do not cache these
        if(cacheControl.find("no-store") != std::string::npos ||
           cacheControl.find("private")  != std::string::npos) {
            throw std::runtime_error("Not cacheable (no-store/private)");
        }

        if(cacheControl.find("must-revalidate") != std::string::npos){
            revalidate = true;
        }

        // Parse max-age
        auto startPos = cacheControl.find("max-age=");
        if(startPos != std::string::npos){
            startPos += 8;
            auto endPos = cacheControl.find(",", startPos);
            if(endPos == std::string::npos) endPos = cacheControl.size();

            std::string maxAgeStr;
            for(size_t i = startPos; i < endPos; i++){
                if(std::isdigit((unsigned char)cacheControl[i])) maxAgeStr += cacheControl[i];
                else break;
            }
            if(!maxAgeStr.empty()){
                int maxAge = std::stoi(maxAgeStr);
                expireTime = std::chrono::system_clock::now() + std::chrono::seconds(maxAge);
            }
        }
    }

    // Expires header can also define expiry time
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
    // Phase 1: shared read only (no LRU mutation)
    Cache cacheCopy;
    {
        std::shared_lock<std::shared_mutex> rlock(cacheMutex);
        auto it = data.find(key);
        if(it == data.end()){
            return CacheStatus::MISS;
        }
        cacheCopy = it->second;
    }

    // Always return cached response/expireTime if present (even if expired/revalidate).
    result = cacheCopy.response;
    expireTimeStr = cacheCopy.getExpireTimeString();

    // Phase 2: update LRU under unique lock (safe)
    {
        std::unique_lock<std::shared_mutex> wlock(cacheMutex);
        if (data.find(key) != data.end()) {
            moveToFrontUnsafe(key);
        }
    }

    if(cacheCopy.isExpired()) return CacheStatus::EXPIRED;
    if(cacheCopy.mustRevalidate) return CacheStatus::REVALIDATE;
    return CacheStatus::VALID;
}

void CacheStore::storeData(const std::string &key, Response response) {
    if(response.status_code != 200) return;

    std::chrono::system_clock::time_point expireTime;
    bool revalidate = false;

    try {
        std::tie(expireTime, revalidate) = parseCacheControl(response.headers);
    } catch (const std::runtime_error &e) {
        Logger::getInstance().logError(0, "Cache skipped: " + std::string(e.what()));
        return;
    }

    std::string eTagVal;
    auto eTagIt = response.headers.find("ETag");
    if(eTagIt != response.headers.end()) eTagVal = eTagIt->second;

    Cache cache(expireTime, revalidate, eTagVal, response);

    std::unique_lock<std::shared_mutex> lock(cacheMutex);

    auto it = data.find(key);
    if(it == data.end()){
        dLlist.push_front(key);
        cacheMap[key] = dLlist.begin();
        data[key] = cache;
    } else {
        it->second = cache;
        moveToFrontUnsafe(key);
    }

    // Enforce LRU eviction
    if(data.size() > MAX_CACHE_SIZE){
        std::string oldKey = dLlist.back();
        dLlist.pop_back();
        data.erase(oldKey);
        cacheMap.erase(oldKey);
    }
}

void CacheStore::updateCacheHeaders(const std::string &key, const Response &newResp) {
    std::unique_lock<std::shared_mutex> lock(cacheMutex);
    auto it = data.find(key);
    if(it == data.end()) return;

    Cache &cached = it->second;

    // Merge headers from 304 response into cached headers
    for(const auto &kv : newResp.headers){
        cached.response.headers[kv.first] = kv.second;
    }

    // Update ETag if present
    auto eTagIt = newResp.headers.find("ETag");
    if(eTagIt != newResp.headers.end()){
        cached.eTag = eTagIt->second;
        cached.response.headers["ETag"] = eTagIt->second;
    }

    // Recompute expiration/revalidate if possible
    try {
        auto [newExpireTime, newRevalidate] = parseCacheControl(cached.response.headers);
        cached.expireTime = newExpireTime;
        cached.mustRevalidate = newRevalidate;
    } catch (const std::runtime_error &e) {
        // If headers indicate non-cacheable, keep old cache entry as-is (safe fallback).
        Logger::getInstance().logError(0, "Cache header update skipped: " + std::string(e.what()));
    }
}
