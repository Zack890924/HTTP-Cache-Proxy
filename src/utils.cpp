#include <string>
#include <netdb.h>
#include <iostream>
#include <unistd.h>
#include <cstring>
#include "utils.hpp"
#include <sstream>
#include <vector>
#include <iomanip>
#include <chrono>

int connectToOther(const std::string &ip, const std::string &portStr){
    struct addrinfo hints, *serverInfo;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    //basic guarantee
    //return error code and ensure the rss would not leak
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
        //basic guarantee
        //return error codec and ensure the rss would not leak
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
    //Strong Guarantee
    //This ensures that req.body is not partially updated when an exception occurs
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



//Strong Guarantee
//It either successfully parses the entire chunked response or does not affect the final_string
std::string handleChunk(const std::string &chunkedBody) {
    std::istringstream stream(chunkedBody);
    std::string final_string;
    std::string line;

    while (std::getline(stream, line)) {
        //space 
        if(line.empty()){
            continue; 
        }
        //remove \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        
        
        size_t chunkSize = 0;
        std::istringstream sizeStream(line);
        sizeStream >> std::hex >> chunkSize;
        if (chunkSize == 0) {
            break;
        }
        
        std::vector<char> buffer(chunkSize);
        stream.read(buffer.data(), chunkSize);
      
        if (stream.gcount() != static_cast<std::streamsize>(chunkSize)) {
            throw std::runtime_error("Size of data did not meet");
        }

        final_string.append(buffer.data(), chunkSize);
        
        if (!std::getline(stream, line)) {
            throw std::runtime_error("Missing CRLF after chunk");
        }
    }
    
    return final_string;
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
    //Strong Guarantee
    //handleChunk() guarantees that res.body is either fully updated or remains unchanged.
    auto te = res.headers.find("Transfer-Encoding");
    if (te != res.headers.end() && te->second == "chunked") {
        std::string chunkedBody;
        char ch;
        while (iss.get(ch)) { 
            chunkedBody += ch;
        }
        res.body = handleChunk(chunkedBody);
    }
    else{
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
    }

    
    return res;


}


//responseToString and requestToString
//Containers such as std::ostringstream do not modify any external state


std::string responseToString(const Response &response){
    std::ostringstream oss;
    oss << response.version << " " << response.status_code << " " << response.status_msg << "\r\n";
    for(auto &header: response.headers){
        oss << header.first << ": " << header.second << "\r\n";
    }
    oss << "\r\n";
    auto it = response.headers.find("Content-Type");
    if (it != response.headers.end() && it->second.find("text/") != std::string::npos) {
        oss << response.body.substr(0, 500);
    } else {
        oss << "[binary data]";
    }
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



std::chrono::system_clock::time_point parseHttpDateToTimePoint(const std::string &httpDate) {

    std::tm tm = {};
    std::istringstream iss(httpDate);

    iss >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S");
    if (iss.fail()) {
        throw std::runtime_error("Failed to parse Expires header");
    }

    time_t tt = timegm(&tm); 
    if (tt == -1) {
        throw std::runtime_error("Failed to convert parsed time to UTC.");
    }
    return std::chrono::system_clock::from_time_t(tt);
}