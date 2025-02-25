#include <string>
#include "utils.hpp"
#include <netinet/in.h>

class connHandler {
    public:
        connHandler(int clientFd);
        ~connHandler();
        void handleConnection();
        void writeToClient(const std::string &response);
    private:
        int clientFd;
        static std::atomic<int> requestCounter;
    



        


};