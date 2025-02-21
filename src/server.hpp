// src/server.hpp

#ifndef SERVER_HPP
#define SERVER_HPP



class Server{
    private:
        int port;
        int serverFd;

        void closeServer(int fd);
        static void signalHandler(int signum);


    public: 
        explicit Server(int port);
        ~Server();

        int get_port() const {
            return port;
        }
    
        int get_serverFd() const {
            return serverFd;
        }
    
        void init();
        int acceptConnection(sockaddr_in &clientAddr, socklen_t &clientAddrSize);
        static void setUpSignalHandler();
};




#endif

