#ifndef CACHE_HPP
#define CACHE_HPP

#include "utils.hpp"
#include <chrono>
#include <string>




class Cache{
    public:
        std::chrono::system_clock::time_point expireTime;
        bool mustRevalidate;
        std::string eTag;
        Response response;

        Cache();
        Cache(std::chrono::system_clock::time_point expireTime, bool mustRevalidate, std::string eTag, Response response);

        bool isExpired() const;
        std::string getExpireTimeString() const;



};

#endif // CACHE_HPP