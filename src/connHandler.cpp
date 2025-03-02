#include "connHandler.hpp"
#include <unistd.h>
#include "proxy.hpp"
#include <arpa/inet.h>
#include "logger.hpp"
#include <sstream>
#include <vector>

std::atomic<int> connHandler::requestCounter{0};

connHandler::connHandler(int clientFd, const sockaddr_in &clientAddr): clientFd(clientFd), clientAddr(clientAddr) {
}


connHandler::~connHandler(){
    if(clientFd >= 0){
        close(clientFd);
    }
    //This destructor is noexcept, ensuring safe resource cleanup.
}


std::string connHandler::getClientIp() const{
    char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(clientAddr.sin_addr), ip_str, INET_ADDRSTRLEN);
    return std::string(ip_str);
}


void connHandler::handleConnection(){
    int reqId = requestCounter.fetch_add(1);

    //receive request
    std::string request;
    std::vector<char> buffer(65536);
    int numBytes = recv(clientFd, buffer.data(), buffer.size() - 1, 0);
    //Receiving data from a client (recv) can fail due to 1. Network errors 2. client disconnection 3. invalid request
    //did not throw exception here; instead, we return a HTTP bad request
    if(numBytes < 0){
        std::string bad_req = "HTTP/1.1 400 Bad Request\r\n";
        sendMesgToClient(bad_req);
        Logger::getInstance().logRespond(reqId, "HTTP/1.1 400 Bad Request");
        return;

    }
    else{
        request = std::string(buffer.data(), numBytes);
    }

    //parse request
    //strong exception safety guarantee
    //If parsing fails, we catch the exception and return an error without modifying the proxy state.
    //Remains in a consistent state as if the request never happened.
    Request req; 
    try{
        req = parseRequest(request);
    } catch(std::exception &e){
        // Logger
        //HTTP/1.1 400 Bad Request
        Logger::getInstance().logRespond(reqId, "HTTP/1.1 400 Bad Request");
        return;

    }

    std::string clientIp = getClientIp();


    //logger
    //123: "GET http://example.com/index.html HTTP/1.1" from 192.168.1.100 @ 2024-02-24 15:00:00 UTC
    {std::ostringstream oss;
    oss << req.method << " " << req.url << " " << req.version;
    Logger::getInstance().logNewRequest(reqId, oss.str(), clientIp);
    }


    //handle reqest
    Proxy proxy;
    
    
    if(req.method == "GET"){
        std::string response = proxy.handleGet(req, reqId, clientIp);
        sendMesgToClient(response);
    }else if(req.method == "POST"){
        std::string response = proxy.handlePost(req, reqId, clientIp);
        sendMesgToClient(response);
    }else if(req.method == "CONNECT"){

        std::string response = proxy.handleConnect(req, reqId, clientIp);
        sendMesgToClient(response);


        std::string host = req.url;
        std::string port = "443";

        size_t col_pos = host.find(':');
        if(col_pos != std::string::npos){
            port = host.substr(col_pos + 1);
            host = host.substr(0, col_pos);
        }

        int serverFd = connectToOther(host, port);
        if(serverFd < 0){
            //logger
            //"HTTP/1.1 502 Bad Gateway
            Logger::getInstance().logRespond(reqId, "HTTP/1.1 502 Bad Gateway");
            sendMesgToClient("HTTP/1.1 502 Bad Gateway\r\n\r\n");
            return;
        }

        doTunnel(serverFd);
        close(serverFd);
        //logger
        //<reqId>: Tunnel closed
        Logger::getInstance().logTunnelClosed(reqId);



    }
    else{
        //logger
        //HTTP/1.1 501 Not Implemented
        Logger::getInstance().logRespond(reqId, "HTTP/1.1 501 Not Implemented");
        sendMesgToClient("HTTP/1.1 501 Not Implemented\r\n\r\n");

    }


}



void connHandler::sendMesgToClient(const std::string &msg){
    send(clientFd, msg.c_str(), msg.size(), 0);
}
void connHandler::doTunnel(int serverFd) {
    int maxFd = (clientFd > serverFd) ? clientFd : serverFd;
    char buf[8192];
    fd_set fds;

    // 為了方便記錄傳輸方向，新增兩個小的 lambda
    auto sendAll = [&](int destFd, const char *data, int len) -> bool {
        int totalSent = 0;
        while (totalSent < len) {
            int n = send(destFd, data + totalSent, len - totalSent, 0);
            if (n < 0) {
                // send 失敗，或被中斷，直接回傳失敗
                Logger::getInstance().logError(0, "sendAll: send() failed");
                return false;
            }
            totalSent += n;
        }
        return true;
    };

    auto nowStr = [](){
        // 取得當前時間字串，方便除錯
        auto now = std::chrono::system_clock::now();
        auto t_c = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&t_c), "%Y-%m-%d %H:%M:%S");
        return oss.str();
    };

    while (true) {
        FD_ZERO(&fds);
        FD_SET(clientFd, &fds);
        FD_SET(serverFd, &fds);

        int ret;
        while (true) {
            ret = select(maxFd + 1, &fds, nullptr, nullptr, nullptr);
            if (ret < 0 && errno == EINTR) {
                // 如果被訊號中斷，繼續重試
                continue;
            }
            break;
        }

        if (ret < 0) {
            // select 發生錯誤
            Logger::getInstance().logError(0, "select failed, closing tunnel.");
            break;
        }

        // 如果 clientFd 可讀
        if (FD_ISSET(clientFd, &fds)) {
            int nBytes = recv(clientFd, buf, sizeof(buf), 0);
            if (nBytes <= 0) {
                // <= 0 表示關閉或錯誤，隧道斷開
                Logger::getInstance().logNote(0, 
                    "doTunnel: clientFd closed or error, nBytes=" + std::to_string(nBytes) + 
                    " at " + nowStr());
                break;
            } else {
                // 嘗試一次把 nBytes 的資料都發給 serverFd
                bool success = sendAll(serverFd, buf, nBytes);
                if (!success) {
                    Logger::getInstance().logError(0, "send to server failed in tunnel.");
                    break;
                }
                Logger::getInstance().logNote(0, 
                    "doTunnel: forwarded " + std::to_string(nBytes) + 
                    " bytes from clientFd to serverFd at " + nowStr());
            }
        }

        // 如果 serverFd 可讀
        if (FD_ISSET(serverFd, &fds)) {
            int nBytes = recv(serverFd, buf, sizeof(buf), 0);
            if (nBytes <= 0) {
                // <= 0 表示關閉或錯誤，隧道斷開
                Logger::getInstance().logNote(0, 
                    "doTunnel: serverFd closed or error, nBytes=" + std::to_string(nBytes) + 
                    " at " + nowStr());
                break;
            } else {
                bool success = sendAll(clientFd, buf, nBytes);
                if (!success) {
                    Logger::getInstance().logError(0, "send to client failed in tunnel.");
                    break;
                }
                Logger::getInstance().logNote(0, 
                    "doTunnel: forwarded " + std::to_string(nBytes) + 
                    " bytes from serverFd to clientFd at " + nowStr());
            }
        }
    }

    // 結束時記錄
    Logger::getInstance().logNote(0, "doTunnel: tunnel loop ended at " + nowStr());
}


void connHandler::doTunnel(int serverFd){
    int maxFd = -1;
    maxFd = (clientFd > serverFd) ? clientFd : serverFd ;
    char buf[8192];
    fd_set fds;

    while(true){
        FD_ZERO(&fds);
        FD_SET(clientFd, &fds);
        FD_SET(serverFd, &fds);
        //basic guarantee
        //If select() fails only logs the error and breaks, but does not roll back the data transfer that has already occurred.
        if(select(maxFd + 1, &fds, nullptr, nullptr, nullptr) < 0){
            Logger::getInstance().logError(0, "select failed, closing tunnel.");
            break;
        }

        //check if ready to read or write
        //basic guarantee
        //If receive() fails only logs the error and breaks, but does not roll back the data transfer that has already occurred.
        if(FD_ISSET(clientFd, &fds)){
            int nBytes = recv(clientFd, buf, sizeof(buf), 0);
            if(nBytes <= 0) {
                break;
            }
            if(send(serverFd, buf, nBytes, 0) < 0){
                Logger::getInstance().logError(0, "send to server failed in tunnel.");
                break;
            }
            
        }
        //basic guarantee
        //If send() fails only logs the error and breaks, but does not roll back the data transfer that has already occurred.
        if(FD_ISSET(serverFd, &fds)){
            int nBytes = recv(serverFd, buf, sizeof(buf), 0);
            if(nBytes <= 0) {
                break;
            }
            if(send(clientFd, buf, nBytes, 0) < 0) {
                Logger::getInstance().logError(0, "send to client failed in tunnel.");
                break;
            }
        }



    }

}








