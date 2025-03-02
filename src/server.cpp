//server.cpp
#include "server.hpp"
#include <netdb.h>
#include <iostream>
#include <unistd.h>
#include <csignal>
#include <cstring>



//no throw 
Server::Server(int port) : port(port), serverFd(-1){}


Server::~Server() {
    if(serverFd != -1){
        closeServer(serverFd);
    }
}

//basic guarantee
//When errors occur, resources are released, and the server state remains consistent.
void Server::init(){
    struct addrinfo hints, *serverInfo;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int status = getaddrinfo(nullptr, std::to_string(port).c_str(), &hints, &serverInfo);
    if(status != 0){
        std::cerr << "getaddrinfo error: " << gai_strerror(status) << std::endl;
        exit(1);
    }

    int opt = 1;
    struct addrinfo *p;
    for(p = serverInfo; p != nullptr; p = p->ai_next){
        serverFd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if(serverFd < 0){
            continue;
        }

        if(setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
            closeServer(serverFd);
            serverFd = -1;
            continue;
        }

        if(bind(serverFd, p->ai_addr, p->ai_addrlen) < 0){
            closeServer(serverFd);
            serverFd = -1;
            continue;
        }
        

        if(listen(serverFd, 10) < 0){
            closeServer(serverFd);
            serverFd = -1;
            continue;
        }
        break;
    }
    freeaddrinfo(serverInfo);

    if(port == 0){
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        if(getsockname(serverFd, (struct sockaddr *)&addr, &len) == 0){
            port = ntohs(addr.sin_port);
        }
        else{
            std::cerr << "getsockname error" << std::endl;
            exit(EXIT_FAILURE);
        }
    }

    if(p == nullptr){
        throw std::runtime_error("Failed to bind");
    }
}


//basic guarantee
//When errors occur, resources are released, and the server state remains consistent.
int Server::acceptConnection(struct sockaddr_in &clientAddr, socklen_t &clientAddrSize){
    
    int clientFd = accept(serverFd, (struct sockaddr *)&clientAddr, &clientAddrSize);
    if(clientFd < 0){
        std::cerr << "accept error" << std::endl;
        exit(EXIT_FAILURE);
    }
    return clientFd;
}

void Server::setUpSignalHandler(){
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = Server::signalHandler;
    sa.sa_flags = SA_RESTART;
    //make sure won't block other signals wihle executing signalHandler
    sigemptyset(&sa.sa_mask);
    if(sigaction(SIGINT, &sa, nullptr) < 0){
        std::cerr << "SIGINT handler error" << std::endl;
        exit(EXIT_FAILURE);
    }
    if(sigaction(SIGTERM, &sa, nullptr) < 0){
        std::cerr << "SIGTERM handler error" << std::endl;
        exit(EXIT_FAILURE);
    }

}


void Server::signalHandler(int signum){
    const char *msg;
    switch(signum){
        case SIGINT:
            msg = "Caught Ctrl+C, shutting down gracefully...\n";
            break;
        case SIGTERM:
            msg = "Caught kill command, shutting down...\n";
            break;
        default:
            msg = "Unknown signal, shutting down...\n";
            break;
    }
    write(STDOUT_FILENO, msg, strlen(msg));
    exit(EXIT_SUCCESS);
}



//basic guarantee
void Server::closeServer(int fd){
    if(close(fd) < 0){
        std::cerr << "close error" << std::endl;
        exit(EXIT_FAILURE);
    }
}