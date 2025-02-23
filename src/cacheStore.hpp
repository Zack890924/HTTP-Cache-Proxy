//ifndef CACHESTORE
//define CACHESTORE

#include "cache.hpp"
#include <map>
#include <mutex>
#include <shared_mutex>


class CacheStore{
    private:
        std::map<std::string, Cache> cacheStore;
        std::shared_mutex cacheMutex;

        CacheStore();

        //get instance
        
        CacheStore(CacheStore const&) = delete;
        CacheStore& operator=(CacheStore const&) = delete;
    

    public:
        static CacheStore& getInstance(){
            static CacheStore instance;
            return instance;
        }
        bool fetchData(const std::string &key, Response &result);
        void storeData(const std::string &key, const Response response);

        
};






//endif // CACHESTORE