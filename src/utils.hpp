#ifndef UTILS_HPP
#define UTILS_HPP
#include <netinet/in.h>  

#include <string>
#include <map>


typedef struct {
    struct sockaddr_in left_addr;
    int left_id;

    struct sockaddr_in right_addr;
    int right_id;
}Neighbor_Info;


typedef struct {
    std::string method;
    std::string url;
    std::string version;
    std::string body;
    std::map<std::string, std::string> headers;

}Request;

typedef struct {
    std::string version;
    int status_code;
    std::string status_msg;
    std::map<std::string, std::string> headers;
    std::string body;
}Response;


int connectToOther(const std::string &ip, const std::string &portStr);
Request parseRequest(const std::string &request);
Response parseResponse(const std::string &response);


#endif // UTILS_HPP