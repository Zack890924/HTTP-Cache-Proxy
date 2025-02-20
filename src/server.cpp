// #src/server.cpp

#include <server.hpp>
#include <netdb.h>
#include <iostream>
#include <unistd.h>
namespace http
{
    ProxyServer::ProxyServer(int port) : port_(port){

    }
    ProxyServer::~ProxyServer(){

    }




    void ProxyServer::setUpSocket(){
        struct addrinfo info, *serverInfo;
        

        memset(&info, 0 ,sizeof(info));
        info.ai_family = AF_UNSPEC;  //IPv4 or IPv6
        info.ai_socktype = SOCK_STREAM;  //TCP
        info.ai_flags = AI_PASSIVE;

        int status = getaddrinfo(nullptr, std::to_string(port_).c_str(), &info, &serverInfo);

        if(status != 0){
            //error
        }

        int opt = 1;
        struct addrinfo *p;
        for(p = serverInfo; p != nullptr; p = p->ai_next){
            serverFd = socket(
                p->ai_family,
                p->ai_socktype,
                p->ai_protocol
            );

            if(serverFd < 0){
                continue;
            }

            status = setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
            if(status < 0){
                close(serverFd);
                serverFd = -1;
                continue;
            }

            status = bind(serverFd, p->ai_addr, p->ai_addrlen);
            if(status < 0){
                close(serverFd);
                serverFd = -1;
                continue;
            }
            break;

        }

        freeaddrinfo(serverInfo);
        if (p == nullptr) {
            throw std::runtime_error("ProxyServer: failed to bind any address");
        }

        status = listen(serverFd, 100);
        if(status < 0){
            close(serverFd);
            serverFd = -1;
            
        }

    

    }

}//namespace http
