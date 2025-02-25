#include <string>
#include <netdb.h>
#include <iostream>
#include <unistd.h>
#include <cstring>
#include "utils.hpp"
#include <sstream>


int connectToOther(const std::string &ip, const std::string &portStr){
    struct addrinfo hints, *serverInfo;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    

    int status = getaddrinfo(ip.c_str(), portStr.c_str(), &hints, &serverInfo);
    if(status != 0){
        std::cerr << "getaddrinfo error: " << gai_strerror(status) << std::endl;
        return -1;
    }

    struct addrinfo *p;
    int socketFd = -1;
    for(p = serverInfo; p != nullptr; p = p->ai_next){
        socketFd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if(socketFd < 0){
            continue;
        }

        if(connect(socketFd, p->ai_addr, p->ai_addrlen) < 0){
            close(socketFd);
            socketFd = -1;
            continue;
        }

        break;
    }
    freeaddrinfo(serverInfo);

    return socketFd;


}


Request parseRequest(const std::string &request){
    std::istringstream iss(request);
    std::string line;
    Request req;

    if(!std::getline(iss, line)){
        throw std::runtime_error("Invalid request: " + line);
    }

    if(!line.empty() && line.back() == '\r'){
        line.pop_back();
    }

    std::istringstream firstLine(line);

    std::string method, url, version;
    firstLine >> method >> url >> version;
    req.method = method;
    req.url = url;  
    req.version = version;
    //header
    while(std::getline(iss, line) && line != "\r" && !line.empty()){
        if(!line.empty() && line.back() == '\r'){
            line.pop_back();
        }

        size_t col_pos = line.find(":");
        if(col_pos == std::string::npos){
            throw std::runtime_error("Invalid header: " + line);
        }else{
            std::string key = line.substr(0, col_pos);
            std::string value = line.substr(col_pos + 1);
            if(!value.empty() && value.front() == ' '){
                value.erase(0,1);
            }
            req.headers[key] = value;
        }
    }

    //body
    auto body_len = req.headers.find("Content-Length");
    if (body_len != req.headers.end()) {
        try {
            int len = std::stoi(body_len->second);
            if (len < 0) throw std::out_of_range("Negative Content-Length");

            std::string body(len, '\0');
            iss.read(&body[0], len);
            req.body = body;
        } catch (const std::exception &e) {
            throw std::runtime_error("Invalid Content-Length");
        }
    } else {
        req.body = "";
    }
    
    return req;
}



Response parseResponse(const std::string &response){
    std::istringstream iss(response);
    std::string line;
    Response res;

    if(!std::getline(iss, line)){
        throw std::runtime_error("Invalid response: " + line);
    }

    if(!line.empty() && line.back() == '\r'){
        line.pop_back();
    }

    //status HTTP/1.1 404 Not Found
    std::istringstream statusLine(line);
    std::string version;
    int status_code;
    std::string status_msg;
    statusLine >> version >> status_code;
    res.status_code = status_code;
    res.version = version;

    //Not Found
    std::getline(statusLine, status_msg);
    if(!status_msg.empty() && status_msg.front() == ' '){
        status_msg.erase(0, status_msg.find_first_not_of(' '));
    }

    res.status_msg = status_msg;



    //headers
    while(std::getline(iss, line) && line != "\r" && !line.empty()){
        if(!line.empty() && line.back() == '\r'){
            line.pop_back();
        }

        size_t col_pos = line.find(":");
        if(col_pos == std::string::npos){
            throw std::runtime_error("Invalid header: " + line);
        }else{
            std::string key = line.substr(0, col_pos);
            std::string value = line.substr(col_pos + 1);
            if(!value.empty() && value.front() == ' '){
                value.erase(0,1);
            }
            res.headers[key] = value;
        }
    }

    //body
    auto body_len = res.headers.find("Content-Length");
    if (body_len != res.headers.end()) {
        try {
            int len = std::stoi(body_len->second);
            if (len < 0) throw std::out_of_range("Negative Content-Length");

            std::string body(len, '\0');
            iss.read(&body[0], len);
            res.body = body;
        } catch (const std::exception &e) {
            throw std::runtime_error("Invalid Content-Length");
        }
    } else {
        res.body = "";
    }
    return res;


}

std::string responseToString(const Response &response){
    std::ostringstream oss;
    oss << response.version << " " << response.status_code << " " << response.status_msg << "\r\n";
    for(auto &header: response.headers){
        oss << header.first << ": " << header.second << "\r\n";
    }
    oss << "\r\n";
    oss << response.body;
    return oss.str();
}


std::string requestToString(const Request &request, const std::string &revalidateHeader){
    std::ostringstream oss;
    oss << request.method << " " << request.url << " " << request.version << "\r\n";
    for(auto &header: request.headers){
        oss << header.first << ": " << header.second << "\r\n";
    }
    if(!revalidateHeader.empty()){
        oss << revalidateHeader;
    }

    oss << "\r\n";
    oss << request.body;
    return oss.str();
}