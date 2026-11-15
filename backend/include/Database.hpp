#pragma once
#include "Config.hpp"
#include <memory>
#include <mutex>
#include <stdexcept>

// MySQL Connector/C++ JDBC API headers
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

class Database {
public:
    explicit Database(const AppConfig& config);
    ~Database() = default;

    // Creates a fresh thread-safe connection for incoming HTTP worker threads
    std::unique_ptr<sql::Connection> getConnection();

    // Verifies connectivity at startup
    bool testConnection();

private:
    AppConfig config_;
    sql::mysql::MySQL_Driver* driver_;
    std::mutex driverMutex_;
};
