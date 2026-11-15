#include "Config.hpp"
#include "Database.hpp"
#include "Utils.hpp"
#include "ApiRoutes.hpp"
#include "httplib.h"
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    std::string configPath = (argc > 1) ? argv[1] : "config/db_config.env";
    AppConfig config = AppConfig::load(configPath);

    std::cout << "==============================================================\n";
    std::cout << "  FlightOps - Flight Scheduling & Airport Management System   \n";
    std::cout << "==============================================================\n";
    std::cout << "[INFO] Target MySQL Host : " << config.dbHost << "\n";
    std::cout << "[INFO] Target Database   : " << config.dbName << "\n";
    std::cout << "[INFO] MySQL Username    : " << config.dbUser << "\n";

    try {
        Database db(config);
        if (!db.testConnection()) {
            std::cerr << "[FATAL] Unable to connect to MySQL database '" << config.dbName << "'.\n"
                      << "        Verify MySQL Server is running and check config/db_config.env.\n";
            return 1;
        }
        std::cout << "[OK]   Connected to MySQL database '" << config.dbName << "'.\n";

        SessionManager sessions;
        httplib::Server server;

        // Register REST API routes
        ApiRoutes::registerAll(server, db, sessions);

        // Locate and serve static frontend files
        std::string frontendPath = config.frontendDir;
        if (!fs::exists(frontendPath) && fs::exists("../" + frontendPath)) {
            frontendPath = "../" + frontendPath;
        }

        if (fs::exists(frontendPath)) {
            server.set_mount_point("/", frontendPath);
            std::cout << "[OK]   Serving Frontend UI from: " << fs::canonical(frontendPath).string() << "\n";
        } else {
            std::cerr << "[WARN] Frontend directory not found at '" << frontendPath << "'. API-only mode.\n";
        }

        std::cout << "[LIVE] Server listening at http://localhost:" << config.serverPort << "/login.html\n";
        std::cout << "       Press Ctrl+C to stop the server.\n";
        std::cout << "==============================================================\n";

        if (!server.listen(config.serverHost, config.serverPort)) {
            std::cerr << "[FATAL] Failed to bind port " << config.serverPort << ". Is another process using it?\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "[FATAL EXCEPTION] " << e.what() << "\n";
        return 1;
    }

    return 0;
}
