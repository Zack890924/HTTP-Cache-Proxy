#include "connHandler.hpp"
#include "proxy.hpp"
#include "utils.hpp"
#include "logger.hpp"

#include <unistd.h>
#include <arpa/inet.h>
#include <sstream>
#include <vector>
#include <cstring>
#include <sys/select.h>

std::atomic<int> connHandler::requestCounter{0};

connHandler::connHandler(int clientFd, const sockaddr_in &clientAddr)
    : clientFd(clientFd), clientAddr(clientAddr) {}

connHandler::~connHandler(){
    if(clientFd >= 0){
        close(clientFd);
    }
    // noexcept destructor: ensures safe resource cleanup
}

std::string connHandler::getClientIp() const{
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(clientAddr.sin_addr), ip_str, INET_ADDRSTRLEN);
    return std::string(ip_str);
}

void connHandler::handleConnection(){
    int reqId = requestCounter.fetch_add(1);

    Request req;
    try{
        // Read a full HTTP request (headers + optional body)
        req = readHttpRequestFromFd(clientFd);
    } catch (...) {
        Logger::getInstance().logRespond(reqId, "HTTP/1.1 400 Bad Request");
        const char *msg = "HTTP/1.1 400 Bad Request\r\n\r\n";
        sendAll(clientFd, msg, strlen(msg));
        return;
    }

    std::string clientIp = getClientIp();

    // Log new request line
    {
        std::ostringstream oss;
        oss << req.method << " " << req.url << " " << req.version;
        Logger::getInstance().logNewRequest(reqId, oss.str(), clientIp);
    }

    Proxy proxy;

    if(req.method == "GET"){
        std::string resp = proxy.handleGet(req, reqId, clientIp);
        sendAll(clientFd, resp.data(), resp.size());
    } else if(req.method == "POST"){
        std::string resp = proxy.handlePost(req, reqId, clientIp);
        sendAll(clientFd, resp.data(), resp.size());
    } else if(req.method == "CONNECT"){
        // Reply 200 first
        std::string resp = proxy.handleConnect(req, reqId, clientIp);
        if(!sendAll(clientFd, resp.data(), resp.size())){
            return;
        }

        // Parse host:port from CONNECT line (req.url)
        std::string host = req.url;
        std::string port = "443";
        size_t col_pos = host.find(':');
        if(col_pos != std::string::npos){
            port = host.substr(col_pos + 1);
            host = host.substr(0, col_pos);
        }

        int serverFd = connectToOther(host, port);
        if(serverFd < 0){
            Logger::getInstance().logRespond(reqId, "HTTP/1.1 502 Bad Gateway");
            const char *msg = "HTTP/1.1 502 Bad Gateway\r\n\r\n";
            sendAll(clientFd, msg, strlen(msg));
            return;
        }

        doTunnel(serverFd);
        close(serverFd);

        Logger::getInstance().logTunnelClosed(reqId);
    } else {
        Logger::getInstance().logRespond(reqId, "HTTP/1.1 501 Not Implemented");
        const char *msg = "HTTP/1.1 501 Not Implemented\r\n\r\n";
        sendAll(clientFd, msg, strlen(msg));
    }
}

void connHandler::doTunnel(int serverFd){
    int maxFd = (clientFd > serverFd) ? clientFd : serverFd;

    char buf[8192];
    fd_set fds;

    while(true){
        FD_ZERO(&fds);
        FD_SET(clientFd, &fds);
        FD_SET(serverFd, &fds);

        // select waits until either side becomes readable
        if(select(maxFd + 1, &fds, nullptr, nullptr, nullptr) < 0){
            Logger::getInstance().logError(0, "select failed, closing tunnel.");
            break;
        }

        // client -> server
        if(FD_ISSET(clientFd, &fds)){
            int nBytes = recv(clientFd, buf, sizeof(buf), 0);
            if(nBytes <= 0) break;
            if(!sendAll(serverFd, buf, (size_t)nBytes)){
                Logger::getInstance().logError(0, "send to server failed in tunnel.");
                break;
            }
        }

        // server -> client
        if(FD_ISSET(serverFd, &fds)){
            int nBytes = recv(serverFd, buf, sizeof(buf), 0);
            if(nBytes <= 0) break;
            if(!sendAll(clientFd, buf, (size_t)nBytes)){
                Logger::getInstance().logError(0, "send to client failed in tunnel.");
                break;
            }
        }
    }
}
