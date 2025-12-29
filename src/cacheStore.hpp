#ifndef CACHESTORE_HPP
#define CACHESTORE_HPP

#include "cache.hpp"
#include <map>
#include <shared_mutex>
#include <list>
#include <utility>

enum class CacheStatus{
    MISS,
    VALID,
    EXPIRED,
    REVALIDATE
};

class CacheStore{
private:
    size_t MAX_CACHE_SIZE;
    std::map<std::string, Cache> data;

    // LRU list: front = most recently used, back = least recently used
    std::list<std::string> dLlist;

    // Key -> iterator into dLlist for O(1) move-to-front
    std::map<std::string, std::list<std::string>::iterator> cacheMap;

    // Protect data + LRU structures
    std::shared_mutex cacheMutex;

    CacheStore();

    // Must be called under unique_lock
    void moveToFrontUnsafe(const std::string &key);

public:
    static CacheStore& getInstance(){
        static CacheStore instance;
        return instance;
    }

    CacheStore(CacheStore const&) = delete;
    CacheStore& operator=(CacheStore const&) = delete;

    CacheStatus fetchData(const std::string &key, Response &result, std::string &expireTimeStr);
    void storeData(const std::string &key, Response response);
    void updateCacheHeaders(const std::string &key, const Response &newResp);

    // Returns {expireTime, mustRevalidate}. Throws if "no-store" or "private".
    std::pair<std::chrono::system_clock::time_point, bool>
    parseCacheControl(const std::map<std::string, std::string>& headers);
};

#endif // CACHESTORE_HPP
