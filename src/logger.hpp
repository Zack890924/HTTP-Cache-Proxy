#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <mutex>
#include <fstream>

class Logger {
private:
    std::mutex mtx;
    std::ofstream ofs;
    Logger();
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    std::string getTimeUTC();

public:
    static Logger& getInstance();

    void logMessage(int requestID, const std::string &msg);
    void logNoteError(int requestID, const std::string &level, const std::string &msg);
    void logNewRequest(int requestID, const std::string &requestLine, const std::string &clientIP);

    void logCacheStatus(int requestID, const std::string &msg);
    void logCacheStatusExpired(int requestID, const std::string &expireTime);

    void logRequesting(int requestID, const std::string &reqLine, const std::string &serverName);
    void logReceived(int requestID, const std::string &respLine, const std::string &serverName);
    void logRespond(int requestID, const std::string &respLine);
    void logTunnelClosed(int requestID);

    void logNotCacheable(int requestID, const std::string &reason);
    void logCachedExpires(int requestID, const std::string &timeStr);
    void logCachedButRevalidate(int requestID);

    void logNote(int requestID, const std::string &msg);
    void logError(int requestID, const std::string &msg);
};

#endif // LOGGER_HPP
