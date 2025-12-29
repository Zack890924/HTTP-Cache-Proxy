#ifndef PROXY_HPP
#define PROXY_HPP

#include "utils.hpp"
#include <string>

class Proxy{
public:
    Proxy();
    ~Proxy();

    std::string handleGet(const Request &req, int requestId, const std::string &clientIp);
    std::string handlePost(const Request &req, int requestId, const std::string &clientIp);
    std::string handleConnect(const Request &req, int requestId, const std::string &clientIp);

private:
    Response forwardToHostResponse(const Request &req, int requestId, const std::string &extraHeaderBlock);
    int connectToHost(const std::string &host, const std::string &portStr);

    // Cache key includes Host to avoid collisions.
    std::string makeCacheKey(const Request &req);
};

#endif // PROXY_HPP
