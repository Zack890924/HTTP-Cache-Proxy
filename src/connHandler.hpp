#ifndef CONNHANDLER_HPP
#define CONNHANDLER_HPP

#include <string>
#include <netinet/in.h>
#include <atomic>

class connHandler {
public:
    connHandler(int clientFd, const sockaddr_in &clientAddr);
    ~connHandler();

    void handleConnection();

private:
    int clientFd;
    const sockaddr_in clientAddr;
    static std::atomic<int> requestCounter;

    std::string getClientIp() const;

    void doTunnel(int serverFd);
};

#endif // CONNHANDLER_HPP
