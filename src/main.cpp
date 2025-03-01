//main.cpp

#include "server.hpp"

#include <iostream>
#include <netinet/in.h>
#include "server.hpp"
#include "threadPool.hpp"
#include "connHandler.hpp"
#include "logger.hpp"

int main(){
    Server::setUpSignalHandler();

    int port = 12345;
    Server server(port);

    try {
        server.init();
        std::cout << "Proxy server listening on port " << server.get_port() << std::endl;
    }
    catch (...) {
        std::cerr << "Server init error: " << std::endl;
        return 1;
    }

    ThreadPool pool(4);

    while (true) {
        sockaddr_in clientAddr;
        socklen_t addrSize = sizeof(clientAddr);
        int clientFd = server.acceptConnection(clientAddr, addrSize);
        if (clientFd < 0) {
            continue;
        }

        pool.enqueue([clientFd, clientAddr]() {
            connHandler handler(clientFd, clientAddr);
            handler.handleConnection();
        });
    }
    return 0;
}
