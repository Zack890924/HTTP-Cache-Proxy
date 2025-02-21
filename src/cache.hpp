//ifndef CACHE_HPP
//#define CACHE_HPP

#include "utils.hpp";



class Cache{
    public:
        std::chrono::system_clock::time_point expireTime;
        bool mustRevalidate;
        std::string eTag;
        Response response;

        Cache();
        Cache(std::chrono::system_clock::time_point expireTime, bool mustRevalidate, std::string eTag, Response response);

        bool isExpired();
        std::string getExpireTime();



};

//endif // CACHE_HPP