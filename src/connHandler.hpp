#include <string>
#include "utils.hpp"
#include <netinet/in.h>

class connHandler {
    public:
        connHandler();
        ~connHandler();
    private:
        int clientFd;
        std::string clientIp;
    public:


        


};