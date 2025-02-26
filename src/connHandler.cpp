#include "connHandler.hpp"
#include <unistd.h>
#include "proxy.hpp"
#include <arpa/inet.h>



connHandler::connHandler(int clientFd, const sockaddr_in &clientAddr): clientFd(clientFd), clientAddr(clientAddr) {
}


connHandler::~connHandler(){
    if(clientFd >= 0){
        close(clientFd);
    }
}


std::string connHandler::getClientIp() const{
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(clientAddr.sin_addr), ip_str, INET_ADDRSTRLEN);
    return std::string(ip_str);
}


void connHandler::handleConnection(){
    int reqId = requestCounter.fetch_add(1);

    //receive request
    std::string request;
    std::vector<char> buffer(65536);
    int numBytes = recv(clientFd, buffer.data(), buffer.size() - 1, 0);
    //TODO
    if(numBytes < 0){
        std::string bad_req = "HTTP/1.1 400 Bad Request\r\n";

    }
    else{
        request = std::string(buffer.data(), numBytes);
    }

    //parse request
    Request req; 
    try{
        req = parseRequest(request);
    } catch(std::exception &e){
        // Logger
        //HTTP/1.1 400 Bad Request
        return;

    }


    //logger
    //123: "GET http://example.com/index.html HTTP/1.1" from 192.168.1.100 @ 2024-02-24 15:00:00 UTC


    //handle reqest
    Proxy proxy;
    std::string clientIp = getClientIp();
    
    if(req.method == "GET"){
        std::string response = proxy.handleGet(req, reqId, clientIp);
    }else if(req.method == "POST"){
        std::string response = proxy.handlePost(req, reqId, clientIp);
    }else if(req.method == "CONNECT"){

        std::string response = proxy.handleConnect(req, reqId, clientIp);
        std::string host = req.url;
        std::string port = "443";

        size_t col_pos = host.find(':');
        if(col_pos != std::string::npos){
            port = host.substr(col_pos + 1);
            host = host.substr(0, col_pos);
        }

        int serverFd = connectToOther(host, port);
        if(serverFd < 0){
            //logger
            //"HTTP/1.1 502 Bad Gateway
        }


    }
    else{
        //TODO

    }


}



void connHandler::doTunnel(int serverFd){
    int maxFd = -1;
    maxFd = (clientFd > serverFd) ? clientFd : serverFd ;
    char buf[8192];
    fd_set fds;

    while(true){
        FD_ZERO(&fds);
        FD_SET(clientFd, &fds);
        FD_SET(serverFd, &fds);

        if(select(maxFd, &fds, nullptr, nullptr, nullptr) < 0){
            break;
        }

        //check if ready to read or write
        if(FD_ISSET(clientFd, &fds)){
            int nBytes = recv(clientFd, buf, sizeof(buf), 0);
            if(nBytes <= 0) {
                break;
            }
            send(clientFd, buf, nBytes, 0);
        }

        if(FD_ISSET(serverFd, &fds)){
            int nBytes = recv(clientFd, buf, sizeof(buf), 0);
            if(nBytes <= 0) {
                break;
            }
            send(clientFd, buf, nBytes, 0);
        }



    }

}








