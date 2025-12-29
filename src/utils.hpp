#ifndef UTILS_HPP
#define UTILS_HPP

#include <netinet/in.h>
#include <string>
#include <map>
#include <chrono>

typedef struct {
    std::string method;
    std::string url;
    std::string version;
    std::string body;
    std::map<std::string, std::string> headers;
} Request;

typedef struct {
    std::string version;
    int status_code;
    std::string status_msg;
    std::map<std::string, std::string> headers;
    std::string body;
} Response;

// Connect helper
int connectToOther(const std::string &ip, const std::string &portStr);

// Robust send (handles partial writes)
bool sendAll(int fd, const char *data, size_t len);

// Read full HTTP request from socket (header + optional body)
Request readHttpRequestFromFd(int clientFd);

// Read full HTTP response from socket (header + body using CL/chunked/close)
Response readHttpResponseFromFd(int serverFd);

// Parsing (these expect a complete message string)
Request parseRequest(const std::string &request);
Response parseResponse(const std::string &response);

// Serialize request to wire bytes (optionally inject extra header lines)
std::string requestToString(const Request &request, const std::string &extraHeaderBlock);

// Serialize response to wire bytes (do NOT truncate; keep binary safe).
// If response was chunked and already decoded, convert to Content-Length.
std::string serializeResponseForClient(Response response);

// String for logging only (may truncate body)
std::string responseToLogString(const Response &response);

// Chunk decoding and HTTP-date parsing
std::string handleChunk(const std::string &chunkedBody);
std::chrono::system_clock::time_point parseHttpDateToTimePoint(const std::string &httpDate);

#endif // UTILS_HPP
