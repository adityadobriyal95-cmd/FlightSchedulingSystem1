#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>

struct AdminSession {
    int adminId;
    std::string username;
    std::string fullName;
    std::string role;
    std::chrono::steady_clock::time_point expiresAt;
};

class Utils {
public:
    // Pure C++ SHA-256 implementation (no OpenSSL dependency headaches on Windows)
    static std::string sha256(const std::string& input);

    // Generates random alphanumeric tokens and PNR booking codes
    static std::string generateToken(size_t length = 48);
    static std::string generateBookingRef();

    // Validation helpers
    static bool isValidDateTime(const std::string& dt); // YYYY-MM-DD HH:MM:SS
    static std::string normalizeDateTime(const std::string& input); // Accepts ISO T format too
    static bool isValidEmail(const std::string& email);
    static bool isValidSeatNumber(const std::string& seat);
    static std::string toUpper(const std::string& s);
};

class SessionManager {
public:
    std::string createSession(int adminId, const std::string& username,
                              const std::string& fullName, const std::string& role);
    bool validateSession(const std::string& token, AdminSession& outSession);
    void removeSession(const std::string& token);

private:
    std::unordered_map<std::string, AdminSession> sessions_;
    std::mutex mutex_;
};
