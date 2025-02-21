#include "connHandler.hpp"
#include <unistd.h>



connHandler::connHandler(int clientFd) : clientFd(clientFd){}
connHandler::~connHandler(){
    if(clientFd >= 0){
        close(clientFd);
    }
}


void connHandler::handleConnection(){
    int reqId = requestCounter++;

    //receive request
    std::string request;
    std::vector<char> buffer(65536);
    int numBytes = recv(clientFd, buffer.data(), buffer.size() - 1, 0);
    //TODO
    if(numBytes < 0){
        return;
    }
    else{
        request = std::string(buffer.data(), numBytes);
    }

    //parse request
    Request req; 
    try{
        req = parseRequest(request);
    } catch(std::exception &e){
        // Logger
        return;

    }


    //handle reqest
    if(req.method == "GET"){
        //TODO
    }else if(req.method == "POST"){
        //TODO  
    }else if(req.method == "CONNECT"){
        //TODO
    }
    else{
        //TODO
    }


}








