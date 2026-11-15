#include "Database.hpp"
#include <iostream>

Database::Database(const AppConfig& config) : config_(config), driver_(nullptr) {
    std::lock_guard<std::mutex> lock(driverMutex_);
    driver_ = sql::mysql::get_mysql_driver_instance();
    if (!driver_) {
        throw std::runtime_error("Failed to initialize MySQL Connector/C++ driver.");
    }
}

std::unique_ptr<sql::Connection> Database::getConnection() {
    std::lock_guard<std::mutex> lock(driverMutex_);
    std::unique_ptr<sql::Connection> conn(
        driver_->connect(config_.dbHost, config_.dbUser, config_.dbPass)
    );
    conn->setSchema(config_.dbName);
    return conn;
}

bool Database::testConnection() {
    try {
        auto conn = getConnection();
        std::unique_ptr<sql::Statement> stmt(conn->createStatement());
        std::unique_ptr<sql::ResultSet> res(stmt->executeQuery("SELECT 1"));
        return res->next();
    } catch (const sql::SQLException& e) {
        std::cerr << "[DB ERROR] " << e.what()
                  << " (MySQL Error Code: " << e.getErrorCode()
                  << ", SQLState: " << e.getSQLState() << ")\n";
        return false;
    }
}
