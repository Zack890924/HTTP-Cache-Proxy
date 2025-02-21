#include <string>
#include "utils.hpp"
#include <netinet/in.h>

class connHandler {
    public:
        connHandler(int clientFd);
        ~connHandler();
        void handleConnection();
        void processGET(const Request &req, int reqId);
    private:
        int clientFd;
        static std::atomic<int> requestCounter;
    



        


};