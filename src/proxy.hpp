//ifndef PROXY_HPP
//define PROXY_HPP


#include "utils.hpp"
#include "server.hpp"


class Proxy{
    public:
        Proxy();
        ~Proxy();
        std::string handleRequest(const std::string &request);
        std::string handleGet(const Request &req, int requestId, const std::string &clientIp);
        std::string handlePost(const Request &req, int requestId, const std::string &clientIp);
        std::string handleConnect(const Request &req, int requestId, const std::string &clientIp);
    private:
        std::string forwardToHost(const Request &req, int requestId, const std::string &clientIp, const std::string &revalidateHeader);
        int connectToHost(const std::string &host, const std::string &portStr);

        
};



//endif // PROXY_HPP