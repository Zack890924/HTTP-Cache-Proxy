#ifndef CACHESTORE_HPP
#define CACHESTORE_HPP

#include "cache.hpp"
#include <map>
#include <mutex>
#include <shared_mutex>
#include <list>



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

        std::list<std::string> dLlist;
        //for faster move to front
        std::map<std::string, std::list<std::string>::iterator> cacheMap;

        std::shared_mutex cacheMutex;

        CacheStore();


        void moveToFront(const std::string &key);
       
        
        
    

    public:
        //get instance
        static CacheStore& getInstance(){
            static CacheStore instance;
            return instance;
        }

        CacheStore(CacheStore const&) = delete;
        CacheStore& operator=(CacheStore const&) = delete;
        CacheStatus fetchData(const std::string &key, Response &result, std::string &expireTimeStr);

        void storeData(const std::string &key, const Response response);

        

        
};






#endif // CACHESTORE_HPP