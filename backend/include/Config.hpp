#pragma once
#include <string>

struct AppConfig {
    std::string dbHost = "tcp://127.0.0.1:3306";
    std::string dbUser = "root";
    std::string dbPass = "";
    std::string dbName = "flight_ops";
    std::string serverHost = "0.0.0.0";
    int serverPort = 8080;
    std::string frontendDir = "frontend";

    static AppConfig load(const std::string& filePath = "config/db_config.env");
};
