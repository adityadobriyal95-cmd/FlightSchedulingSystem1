#include "Utils.hpp"
#include <array>
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <random>
#include <regex>
#include <algorithm>
#include <cctype>

namespace {
constexpr std::array<uint32_t, 64> K = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

inline uint32_t rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32 - n));
}
}

std::string Utils::sha256(const std::string& input) {
    uint32_t h[8] = {
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
    };

    std::vector<uint8_t> data(input.begin(), input.end());
    uint64_t bitLen = static_cast<uint64_t>(data.size()) * 8;
    data.push_back(0x80);
    while ((data.size() % 64) != 56) {
        data.push_back(0x00);
    }
    for (int i = 7; i >= 0; --i) {
        data.push_back(static_cast<uint8_t>((bitLen >> (i * 8)) & 0xFF));
    }

    for (size_t offset = 0; offset < data.size(); offset += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(data[offset + i * 4]) << 24) |
                   (static_cast<uint32_t>(data[offset + i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(data[offset + i * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(data[offset + i * 4 + 3]));
        }
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
        uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];

        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ ((~e) & g);
            uint32_t temp1 = hh + S1 + ch + K[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = S0 + maj;

            hh = g; g = f; f = e; e = d + temp1;
            d = c; c = b; b = a; a = temp1 + temp2;
        }

        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }

    std::ostringstream oss;
    for (int i = 0; i < 8; ++i) {
        oss << std::hex << std::setfill('0') << std::setw(8) << h[i];
    }
    return oss.str();
}

std::string Utils::generateToken(size_t length) {
    static const char charset[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);
    std::string token;
    token.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        token.push_back(charset[dist(gen)]);
    }
    return token;
}

std::string Utils::generateBookingRef() {
    static const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(0, sizeof(charset) - 2);
    std::string ref = "PNR";
    for (int i = 0; i < 5; ++i) {
        ref.push_back(charset[dist(gen)]);
    }
    return ref;
}

std::string Utils::normalizeDateTime(const std::string& input) {
    std::string s = input;
    std::replace(s.begin(), s.end(), 'T', ' ');
    if (s.size() == 16) {
        s += ":00";
    }
    return s;
}

bool Utils::isValidDateTime(const std::string& dt) {
    static const std::regex pattern(R"(^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$)");
    return std::regex_match(dt, pattern);
}

bool Utils::isValidEmail(const std::string& email) {
    static const std::regex pattern(R"(^[A-Za-z0-9._%+\-]+@[A-Za-z0-9.\-]+\.[A-Za-z]{2,}$)");
    return std::regex_match(email, pattern);
}

bool Utils::isValidSeatNumber(const std::string& seat) {
    static const std::regex pattern(R"(^\d{1,2}[A-K]$)");
    return std::regex_match(seat, pattern);
}

std::string Utils::toUpper(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

std::string SessionManager::createSession(int adminId, const std::string& username,
                                          const std::string& fullName, const std::string& role) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::string token = Utils::generateToken(48);
    AdminSession session{
        adminId,
        username,
        fullName,
        role,
        std::chrono::steady_clock::now() + std::chrono::hours(12)
    };
    sessions_[token] = session;
    return token;
}

bool SessionManager::validateSession(const std::string& token, AdminSession& outSession) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sessions_.find(token);
    if (it == sessions_.end()) return false;
    if (std::chrono::steady_clock::now() > it->second.expiresAt) {
        sessions_.erase(it);
        return false;
    }
    outSession = it->second;
    return true;
}

void SessionManager::removeSession(const std::string& token) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(token);
}
