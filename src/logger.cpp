#include "logger.hpp"
#include <iostream>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

static const char* LOG_PATH = "/var/log/erss/proxy.log";

Logger::Logger() {
    ofs.open(LOG_PATH, std::ios::app);
    if (!ofs.is_open()) {
        std::cerr << "Cannot open log file: " << LOG_PATH << std::endl;
    }
}

Logger::~Logger() {
    if (ofs.is_open()) {
        ofs.close();
    }
}

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

//get UTC time
std::string Logger::getTimeUTC() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    std::tm tm = *std::gmtime(&now_time);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S UTC");
    return oss.str();
}

//general log method
void Logger::logMessage(int requestID, const std::string &msg) {
    std::lock_guard<std::mutex> lock(mtx);
    if (ofs.is_open()) {
        ofs << requestID << ": " << msg << std::endl;
    }
}

//"GET /index.html HTTP/1.1" from 192.168.1.10 @ 2025-02-27 15:45:30 UTC
void Logger::logNewRequest(int requestID, const std::string &requestLine, const std::string &clientIP) {
    std::ostringstream oss;
    oss << "\"" << requestLine << "\" from " << clientIP << " @ " << getTimeUTC();
    logMessage(requestID, oss.str());
}

//105: not in cache
//106: in cache, valid
//107: in cache, requires validation
void Logger::logCacheStatus(int requestID, const std::string &msg) {
    logMessage(requestID, msg);
}

// 104: Requesting "GET /index.html HTTP/1.1" from www.example.com
void Logger::logRequesting(int requestID, const std::string &reqLine, const std::string &serverName) {
    std::ostringstream oss;
    oss << "Requesting \"" << reqLine << "\" from " << serverName;
    logMessage(requestID, oss.str());
}

//104: Received "HTTP/1.1 200 OK" from www.example.com
void Logger::logReceived(int requestID, const std::string &respLine, const std::string &serverName) {
    std::ostringstream oss;
    oss << "Received \"" << respLine << "\" from " << serverName;
    logMessage(requestID, oss.str());
}

//104: Responding "HTTP/1.1 200 OK"
void Logger::logRespond(int requestID, const std::string &respLine) {
    std::ostringstream oss;
    oss << "Responding \"" << respLine << "\"";
    logMessage(requestID, oss.str());
}

// 108: Tunnel closed
void Logger::logTunnelClosed(int requestID) {
    logMessage(requestID, "Tunnel closed");
}

// 109: not cacheable because reason
void Logger::logNotCacheable(int requestID, const std::string &reason) {
    std::ostringstream oss;
    oss << "not cacheable because " << reason;
    logMessage(requestID, oss.str());
}

// 110: cached, expires at Sun Feb 27 12:00:00 2025 UTC
void Logger::logCachedExpires(int requestID, const std::string &timeStr) {
    std::ostringstream oss;
    oss << "cached, expires at " << timeStr;
    logMessage(requestID, oss.str());
}

// 111: cached, but requires re-validation
void Logger::logCachedButRevalidate(int requestID) {
    logMessage(requestID, "cached, but requires re-validation");
}

void Logger::logCacheStatusExpired(int requestID, const std::string &expireTime) {
    logMessage(requestID, "in cache, but expired at " + expireTime);
}


// NOTE / ERROR
//104: NOTE Cache-Control: must-revalidate/
//104: NOTE ETag: W/"33bc8-F9Kn1zgYX0cHOaRFsmZORA"
void Logger::logNoteError(int requestID, const std::string &level, const std::string &msg) {
    std::ostringstream oss;
    oss << level << " " << msg;
    logMessage(requestID, oss.str());
}

void Logger::logNote(int requestID, const std::string &msg) {
    logNoteError(requestID, "NOTE", msg);
}

void Logger::logError(int requestID, const std::string &msg) {
    logNoteError(requestID, "ERROR", msg);
}
