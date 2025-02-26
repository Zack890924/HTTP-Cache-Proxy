#include <string>
#include "utils.hpp"
#include <netinet/in.h>

class connHandler {
    public:
        connHandler(int clientFd, const sockaddr_in &clientAddr);
        ~connHandler();
        void handleConnection();
        void writeToClient(const std::string &response);
    private:
        int clientFd;
        const sockaddr_in clientAddr;
        static std::atomic<int> requestCounter;
        std::string getClientIp() const;
        void doTunnel(int serverFd);

    



        


};