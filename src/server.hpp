// src/server.hpp

#ifndef SERVER_HPP
#define SERVER_HPP


namespace http
{
    class ProxyServer{
        private:
            int port_;
            int serverFd;

        public: 
            ProxyServer(int port);
            ~ProxyServer();

            void setUpSocket();
    };

}


#endif

