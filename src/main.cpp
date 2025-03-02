//my_proxy/src/main.cpp

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
    //basic guarentee
    //When an exception occurs, the server can maintain a consistent state
    //without achieving the strong guarantee of "not changing the state at all".
    try {
        server.init();
        std::cout << "Proxy server listening on port " << server.get_port() << std::endl;
    }
    catch (const std::exception &e) {
        std::cerr << "Server init error: " << e.what() << std::endl;
        return 1;
      }
      

    ThreadPool pool(4);

    while (true) {
        //basic guarantee
        //If errors are encountered, they will not cause the entire service to crash.
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
