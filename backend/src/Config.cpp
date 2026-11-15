#include "Config.hpp"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <iostream>
#include <algorithm>

namespace {
std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}
}

AppConfig AppConfig::load(const std::string& filePath) {
    AppConfig cfg;

    // 1. Try loading from config file if present
    std::ifstream file(filePath);
    if (!file.is_open()) {
        // Also try parent relative path if launched from build/ directory
        file.open("../" + filePath);
    }

    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;
            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;

            std::string key = trim(line.substr(0, eq));
            std::string val = trim(line.substr(eq + 1));

            if (key == "DB_HOST") cfg.dbHost = val;
            else if (key == "DB_USER") cfg.dbUser = val;
            else if (key == "DB_PASS") cfg.dbPass = val;
            else if (key == "DB_NAME") cfg.dbName = val;
            else if (key == "SERVER_HOST") cfg.serverHost = val;
            else if (key == "SERVER_PORT") cfg.serverPort = std::stoi(val);
            else if (key == "FRONTEND_DIR") cfg.frontendDir = val;
        }
    } else {
        std::cerr << "[WARN] Config file not found at " << filePath
                  << ". Falling back to environment variables.\n";
    }

    // 2. Environment variables override file values if set
    if (const char* v = std::getenv("DB_HOST")) cfg.dbHost = v;
    if (const char* v = std::getenv("DB_USER")) cfg.dbUser = v;
    if (const char* v = std::getenv("DB_PASS")) cfg.dbPass = v;
    if (const char* v = std::getenv("DB_NAME")) cfg.dbName = v;
    if (const char* v = std::getenv("SERVER_HOST")) cfg.serverHost = v;
    if (const char* v = std::getenv("SERVER_PORT")) cfg.serverPort = std::stoi(v);
    if (const char* v = std::getenv("FRONTEND_DIR")) cfg.frontendDir = v;

    return cfg;
}
