#include "ApiRoutes.hpp"
#include "ConflictDetector.hpp"
#include "FlightScheduler.hpp"
#include "json.hpp"
#include <iostream>
#include <memory>

using json = nlohmann::json;

namespace {

void sendJson(httplib::Response& res, int status, const json& payload) {
    res.status = status;
    res.set_content(payload.dump(), "application/json; charset=utf-8");
}

std::string extractBearerToken(const httplib::Request& req) {
    if (!req.has_header("Authorization")) return "";
    std::string auth = req.get_header_value("Authorization");
    const std::string prefix = "Bearer ";
    if (auth.rfind(prefix, 0) == 0) {
        return auth.substr(prefix.size());
    }
    return "";
}

bool requireAuth(const httplib::Request& req, httplib::Response& res,
                 SessionManager& sessions, AdminSession& admin) {
    std::string token = extractBearerToken(req);
    if (token.empty() || !sessions.validateSession(token, admin)) {
        sendJson(res, 401, {{"success", false}, {"error", "Unauthorized. Please log in."}});
        return false;
    }
    return true;
}

} // namespace

void ApiRoutes::registerAll(httplib::Server& server, Database& db, SessionManager& sessions) {

    // Global CORS & preflight handling
    server.Options(R"(/api/.*)", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        res.status = 204;
    });

    server.set_post_routing_handler([](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
    });

    // Health Check
    server.Get("/api/health", [&db](const httplib::Request&, httplib::Response& res) {
        bool ok = db.testConnection();
        sendJson(res, ok ? 200 : 503, {
            {"success", ok},
            {"service", "FlightOps C++ Backend"},
            {"database", ok ? "CONNECTED" : "DISCONNECTED"}
        });
    });

    // =========================================================================
    // 1. AUTHENTICATION ENDPOINTS
    // =========================================================================
    server.Post("/api/auth/login", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string username = body.value("username", "");
            std::string password = body.value("password", "");

            if (username.empty() || password.empty()) {
                sendJson(res, 400, {{"success", false}, {"error", "Username and password are required."}});
                return;
            }

            std::string hashed = Utils::sha256(password);
            auto conn = db.getConnection();
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                "SELECT admin_id, username, full_name, role FROM admins "
                "WHERE username = ? AND password_hash = ?"
            ));
            pstmt->setString(1, username);
            pstmt->setString(2, hashed);
            std::unique_ptr<sql::ResultSet> rs(pstmt->executeQuery());

            if (!rs->next()) {
                sendJson(res, 401, {{"success", false}, {"error", "Invalid username or password."}});
                return;
            }

            int adminId = rs->getInt("admin_id");
            std::string uname = rs->getString("username");
            std::string fullName = rs->getString("full_name");
            std::string role = rs->getString("role");
            std::string token = sessions.createSession(adminId, uname, fullName, role);

            sendJson(res, 200, {
                {"success", true},
                {"token", token},
                {"admin", {
                    {"admin_id", adminId},
                    {"username", uname},
                    {"full_name", fullName},
                    {"role", role}
                }}
            });
        } catch (const std::exception& e) {
            sendJson(res, 500, {{"success", false}, {"error", e.what()}});
        }
    });

    server.Post("/api/auth/logout", [&sessions](const httplib::Request& req, httplib::Response& res) {
        std::string token = extractBearerToken(req);
        if (!token.empty()) sessions.removeSession(token);
        sendJson(res, 200, {{"success", true}, {"message", "Logged out successfully."}});
    });

    server.Get("/api/auth/me", [&sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        sendJson(res, 200, {
            {"success", true},
            {"admin", {
                {"admin_id", admin.adminId},
                {"username", admin.username},
                {"full_name", admin.fullName},
                {"role", admin.role}
            }}
        });
    });

    // =========================================================================
    // 2. AIRPORT MANAGEMENT (CRUD + Runways/Gates sub-resources)
    // =========================================================================
    server.Get("/api/airports", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            auto conn = db.getConnection();
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                "SELECT a.*, "
                "(SELECT COUNT(*) FROM runways r WHERE r.airport_id = a.airport_id) AS runway_count, "
                "(SELECT COUNT(*) FROM gates g WHERE g.airport_id = a.airport_id) AS gate_count "
                "FROM airports a ORDER BY a.iata_code ASC"
            ));
            std::unique_ptr<sql::ResultSet> rs(pstmt->executeQuery());
            json list = json::array();
            while (rs->next()) {
                list.push_back({
                    {"airport_id", rs->getInt("airport_id")},
                    {"iata_code", rs->getString("iata_code")},
                    {"icao_code", rs->getString("icao_code")},
                    {"name", rs->getString("name")},
                    {"city", rs->getString("city")},
                    {"country", rs->getString("country")},
                    {"timezone", rs->getString("timezone")},
                    {"status", rs->getString("status")},
                    {"runway_count", rs->getInt("runway_count")},
                    {"gate_count", rs->getInt("gate_count")}
                });
            }
            sendJson(res, 200, {{"success", true}, {"data", list}});
        } catch (const std::exception& e) {
            sendJson(res, 500, {{"success", false}, {"error", e.what()}});
        }
    });

    server.Post("/api/airports", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            auto body = json::parse(req.body);
            std::string iata = Utils::toUpper(body.value("iata_code", ""));
            std::string icao = Utils::toUpper(body.value("icao_code", ""));
            std::string name = body.value("name", "");
            std::string city = body.value("city", "");
            std::string country = body.value("country", "");
            std::string timezone = body.value("timezone", "UTC");
            std::string status = body.value("status", "OPERATIONAL");

            if (iata.size() != 3 || icao.size() != 4 || name.empty() || city.empty() || country.empty()) {
                sendJson(res, 400, {{"success", false}, {"error", "Valid 3-letter IATA, 4-letter ICAO, name, city, and country are required."}});
                return;
            }

            auto conn = db.getConnection();
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                "INSERT INTO airports (iata_code, icao_code, name, city, country, timezone, status) "
                "VALUES (?, ?, ?, ?, ?, ?, ?)"
            ));
            pstmt->setString(1, iata);
            pstmt->setString(2, icao);
            pstmt->setString(3, name);
            pstmt->setString(4, city);
            pstmt->setString(5, country);
            pstmt->setString(6, timezone);
            pstmt->setString(7, status);
            pstmt->executeUpdate();

            // Automatically provision default Runway 09/27 and Gate A1 so new airports are immediately usable
            std::unique_ptr<sql::Statement> idStmt(conn->createStatement());
            std::unique_ptr<sql::ResultSet> idRes(idStmt->executeQuery("SELECT LAST_INSERT_ID() AS id"));
            if (idRes->next()) {
                int newAirportId = idRes->getInt("id");
                std::unique_ptr<sql::PreparedStatement> rStmt(conn->prepareStatement(
                    "INSERT INTO runways (airport_id, runway_code, length_meters, surface_type, status) VALUES (?, '09/27', 3500, 'ASPHALT', 'ACTIVE')"
                ));
                rStmt->setInt(1, newAirportId);
                rStmt->executeUpdate();

                std::unique_ptr<sql::PreparedStatement> gStmt(conn->prepareStatement(
                    "INSERT INTO gates (airport_id, terminal, gate_code, gate_type, status) VALUES (?, 'T1', 'A1', 'DOMESTIC', 'AVAILABLE')"
                ));
                gStmt->setInt(1, newAirportId);
                gStmt->executeUpdate();
            }

            sendJson(res, 201, {{"success", true}, {"message", "Airport created with default runway 09/27 and gate A1."}});
        } catch (const sql::SQLException& e) {
            sendJson(res, 400, {{"success", false}, {"error", std::string("Database constraint error: ") + e.what()}});
        } catch (const std::exception& e) {
            sendJson(res, 500, {{"success", false}, {"error", e.what()}});
        }
    });

    server.Put(R"(/api/airports/(\d+))", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            int id = std::stoi(req.matches[1]);
            auto body = json::parse(req.body);
            auto conn = db.getConnection();
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                "UPDATE airports SET iata_code = ?, icao_code = ?, name = ?, city = ?, country = ?, timezone = ?, status = ? "
                "WHERE airport_id = ?"
            ));
            pstmt->setString(1, Utils::toUpper(body.value("iata_code", "")));
            pstmt->setString(2, Utils::toUpper(body.value("icao_code", "")));
            pstmt->setString(3, body.value("name", ""));
            pstmt->setString(4, body.value("city", ""));
            pstmt->setString(5, body.value("country", ""));
            pstmt->setString(6, body.value("timezone", "UTC"));
            pstmt->setString(7, body.value("status", "OPERATIONAL"));
            pstmt->setInt(8, id);
            int rows = pstmt->executeUpdate();
            if (rows == 0) {
                sendJson(res, 404, {{"success", false}, {"error", "Airport not found."}});
                return;
            }
            sendJson(res, 200, {{"success", true}, {"message", "Airport updated successfully."}});
        } catch (const std::exception& e) {
            sendJson(res, 400, {{"success", false}, {"error", e.what()}});
        }
    });

    server.Delete(R"(/api/airports/(\d+))", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            int id = std::stoi(req.matches[1]);
            auto conn = db.getConnection();
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                "DELETE FROM airports WHERE airport_id = ?"
            ));
            pstmt->setInt(1, id);
            int rows = pstmt->executeUpdate();
            if (rows == 0) {
                sendJson(res, 404, {{"success", false}, {"error", "Airport not found."}});
                return;
            }
            sendJson(res, 200, {{"success", true}, {"message", "Airport deleted."}});
        } catch (const sql::SQLException&) {
            sendJson(res, 409, {{"success", false}, {"error", "Cannot delete airport with active scheduled flights or stationed aircraft."}});
        }
    });

    // Runways & Gates listing + creation
    server.Get("/api/resources", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            auto conn = db.getConnection();
            json runways = json::array();
            {
                std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                    "SELECT r.*, a.iata_code FROM runways r JOIN airports a ON r.airport_id = a.airport_id ORDER BY a.iata_code, r.runway_code"
                ));
                std::unique_ptr<sql::ResultSet> rs(pstmt->executeQuery());
                while (rs->next()) {
                    runways.push_back({
                        {"runway_id", rs->getInt("runway_id")},
                        {"airport_id", rs->getInt("airport_id")},
                        {"iata_code", rs->getString("iata_code")},
                        {"runway_code", rs->getString("runway_code")},
                        {"length_meters", rs->getInt("length_meters")},
                        {"surface_type", rs->getString("surface_type")},
                        {"status", rs->getString("status")}
                    });
                }
            }

            json gates = json::array();
            {
                std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                    "SELECT g.*, a.iata_code FROM gates g JOIN airports a ON g.airport_id = a.airport_id ORDER BY a.iata_code, g.terminal, g.gate_code"
                ));
                std::unique_ptr<sql::ResultSet> rs(pstmt->executeQuery());
                while (rs->next()) {
                    gates.push_back({
                        {"gate_id", rs->getInt("gate_id")},
                        {"airport_id", rs->getInt("airport_id")},
                        {"iata_code", rs->getString("iata_code")},
                        {"terminal", rs->getString("terminal")},
                        {"gate_code", rs->getString("gate_code")},
                        {"gate_type", rs->getString("gate_type")},
                        {"status", rs->getString("status")}
                    });
                }
            }
            sendJson(res, 200, {{"success", true}, {"runways", runways}, {"gates", gates}});
        } catch (const std::exception& e) {
            sendJson(res, 500, {{"success", false}, {"error", e.what()}});
        }
    });

    server.Post("/api/runways", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            auto body = json::parse(req.body);
            auto conn = db.getConnection();
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                "INSERT INTO runways (airport_id, runway_code, length_meters, surface_type, status) VALUES (?, ?, ?, ?, ?)"
            ));
            pstmt->setInt(1, body.value("airport_id", 0));
            pstmt->setString(2, Utils::toUpper(body.value("runway_code", "")));
            pstmt->setInt(3, body.value("length_meters", 3000));
            pstmt->setString(4, body.value("surface_type", "ASPHALT"));
            pstmt->setString(5, body.value("status", "ACTIVE"));
            pstmt->executeUpdate();
            sendJson(res, 201, {{"success", true}, {"message", "Runway added."}});
        } catch (const std::exception& e) {
            sendJson(res, 400, {{"success", false}, {"error", e.what()}});
        }
    });

    server.Post("/api/gates", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            auto body = json::parse(req.body);
            auto conn = db.getConnection();
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                "INSERT INTO gates (airport_id, terminal, gate_code, gate_type, status) VALUES (?, ?, ?, ?, ?)"
            ));
            pstmt->setInt(1, body.value("airport_id", 0));
            pstmt->setString(2, Utils::toUpper(body.value("terminal", "T1")));
            pstmt->setString(3, Utils::toUpper(body.value("gate_code", "")));
            pstmt->setString(4, body.value("gate_type", "DOMESTIC"));
            pstmt->setString(5, body.value("status", "AVAILABLE"));
            pstmt->executeUpdate();
            sendJson(res, 201, {{"success", true}, {"message", "Gate added."}});
        } catch (const std::exception& e) {
            sendJson(res, 400, {{"success", false}, {"error", e.what()}});
        }
    });

    // =========================================================================
    // 3. AIRCRAFT MANAGEMENT (CRUD + Availability Tracking)
    // =========================================================================
    server.Get("/api/aircraft", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            auto conn = db.getConnection();
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                "SELECT ac.*, ap.iata_code AS current_airport_iata, "
                "(SELECT COUNT(*) FROM flights f WHERE f.aircraft_id = ac.aircraft_id AND f.status IN ('SCHEDULED','BOARDING','DEPARTED','IN_AIR','DELAYED')) AS active_flights "
                "FROM aircraft ac LEFT JOIN airports ap ON ac.current_airport_id = ap.airport_id "
                "ORDER BY ac.registration_no ASC"
            ));
            std::unique_ptr<sql::ResultSet> rs(pstmt->executeQuery());
            json list = json::array();
            while (rs->next()) {
                list.push_back({
                    {"aircraft_id", rs->getInt("aircraft_id")},
                    {"registration_no", rs->getString("registration_no")},
                    {"model", rs->getString("model")},
                    {"manufacturer", rs->getString("manufacturer")},
                    {"total_seats", rs->getInt("total_seats")},
                    {"range_km", rs->getInt("range_km")},
                    {"status", rs->getString("status")},
                    {"current_airport_id", rs->isNull("current_airport_id") ? 0 : rs->getInt("current_airport_id")},
                    {"current_airport_iata", rs->isNull("current_airport_iata") ? "UNASSIGNED" : rs->getString("current_airport_iata")},
                    {"active_flights", rs->getInt("active_flights")}
                });
            }
            sendJson(res, 200, {{"success", true}, {"data", list}});
        } catch (const std::exception& e) {
            sendJson(res, 500, {{"success", false}, {"error", e.what()}});
        }
    });

    server.Post("/api/aircraft", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            auto body = json::parse(req.body);
            std::string reg = Utils::toUpper(body.value("registration_no", ""));
            std::string model = body.value("model", "");
            std::string mfg = body.value("manufacturer", "");
            int seats = body.value("total_seats", 180);
            int rangeKm = body.value("range_km", 5000);
            std::string status = body.value("status", "AVAILABLE");
            int airportId = body.value("current_airport_id", 0);

            if (reg.empty() || model.empty() || mfg.empty() || seats < 10) {
                sendJson(res, 400, {{"success", false}, {"error", "Registration, model, manufacturer, and valid seat count (>=10) are required."}});
                return;
            }

            auto conn = db.getConnection();
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                "INSERT INTO aircraft (registration_no, model, manufacturer, total_seats, range_km, status, current_airport_id) "
                "VALUES (?, ?, ?, ?, ?, ?, ?)"
            ));
            pstmt->setString(1, reg);
            pstmt->setString(2, model);
            pstmt->setString(3, mfg);
            pstmt->setInt(4, seats);
            pstmt->setInt(5, rangeKm);
            pstmt->setString(6, status);
            if (airportId > 0) pstmt->setInt(7, airportId);
            else pstmt->setNull(7, sql::DataType::INTEGER);
            pstmt->executeUpdate();

            sendJson(res, 201, {{"success", true}, {"message", "Aircraft added to fleet."}});
        } catch (const std::exception& e) {
            sendJson(res, 400, {{"success", false}, {"error", e.what()}});
        }
    });

    server.Put(R"(/api/aircraft/(\d+))", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            int id = std::stoi(req.matches[1]);
            auto body = json::parse(req.body);
            auto conn = db.getConnection();
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                "UPDATE aircraft SET registration_no = ?, model = ?, manufacturer = ?, total_seats = ?, "
                "range_km = ?, status = ?, current_airport_id = ? WHERE aircraft_id = ?"
            ));
            pstmt->setString(1, Utils::toUpper(body.value("registration_no", "")));
            pstmt->setString(2, body.value("model", ""));
            pstmt->setString(3, body.value("manufacturer", ""));
            pstmt->setInt(4, body.value("total_seats", 180));
            pstmt->setInt(5, body.value("range_km", 5000));
            pstmt->setString(6, body.value("status", "AVAILABLE"));
            int airportId = body.value("current_airport_id", 0);
            if (airportId > 0) pstmt->setInt(7, airportId);
            else pstmt->setNull(7, sql::DataType::INTEGER);
            pstmt->setInt(8, id);

            if (pstmt->executeUpdate() == 0) {
                sendJson(res, 404, {{"success", false}, {"error", "Aircraft not found."}});
                return;
            }
            sendJson(res, 200, {{"success", true}, {"message", "Aircraft updated."}});
        } catch (const std::exception& e) {
            sendJson(res, 400, {{"success", false}, {"error", e.what()}});
        }
    });

    server.Delete(R"(/api/aircraft/(\d+))", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            int id = std::stoi(req.matches[1]);
            auto conn = db.getConnection();
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                "DELETE FROM aircraft WHERE aircraft_id = ?"
            ));
            pstmt->setInt(1, id);
            if (pstmt->executeUpdate() == 0) {
                sendJson(res, 404, {{"success", false}, {"error", "Aircraft not found."}});
                return;
            }
            sendJson(res, 200, {{"success", true}, {"message", "Aircraft removed."}});
        } catch (const sql::SQLException&) {
            sendJson(res, 409, {{"success", false}, {"error", "Cannot delete aircraft assigned to existing flights."}});
        }
    });

    // =========================================================================
    // 4. FLIGHT MANAGEMENT (Search/Filter, Create, Edit, Cancel + Conflict Check)
    // =========================================================================
    server.Get("/api/flights", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            std::string search = req.has_param("search") ? req.get_param_value("search") : "";
            std::string status = req.has_param("status") ? req.get_param_value("status") : "";
            std::string origin = req.has_param("origin") ? req.get_param_value("origin") : "";

            std::string sqlQuery =
                "SELECT f.*, "
                "o.iata_code AS origin_iata, o.city AS origin_city, "
                "d.iata_code AS dest_iata, d.city AS dest_city, "
                "ac.registration_no, ac.model AS aircraft_model, ac.total_seats, "
                "dg.gate_code AS dep_gate_code, ag.gate_code AS arr_gate_code, "
                "dr.runway_code AS dep_runway_code, ar.runway_code AS arr_runway_code, "
                "(SELECT COUNT(*) FROM bookings b WHERE b.flight_id = f.flight_id AND b.booking_status = 'CONFIRMED') AS booked_seats "
                "FROM flights f "
                "JOIN airports o ON f.origin_airport_id = o.airport_id "
                "JOIN airports d ON f.destination_airport_id = d.airport_id "
                "JOIN aircraft ac ON f.aircraft_id = ac.aircraft_id "
                "JOIN gates dg ON f.departure_gate_id = dg.gate_id "
                "JOIN gates ag ON f.arrival_gate_id = ag.gate_id "
                "JOIN runways dr ON f.departure_runway_id = dr.runway_id "
                "JOIN runways ar ON f.arrival_runway_id = ar.runway_id "
                "WHERE 1=1 ";

            if (!search.empty()) {
                sqlQuery += "AND (f.flight_number LIKE ? OR f.airline_name LIKE ? OR o.city LIKE ? OR d.city LIKE ?) ";
            }
            if (!status.empty() && status != "ALL") {
                sqlQuery += "AND f.status = ? ";
            }
            if (!origin.empty() && origin != "ALL") {
                sqlQuery += "AND o.iata_code = ? ";
            }
            sqlQuery += "ORDER BY f.scheduled_departure ASC";

            auto conn = db.getConnection();
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(sqlQuery));
            int paramIdx = 1;
            if (!search.empty()) {
                std::string like = "%" + search + "%";
                pstmt->setString(paramIdx++, like);
                pstmt->setString(paramIdx++, like);
                pstmt->setString(paramIdx++, like);
                pstmt->setString(paramIdx++, like);
            }
            if (!status.empty() && status != "ALL") {
                pstmt->setString(paramIdx++, status);
            }
            if (!origin.empty() && origin != "ALL") {
                pstmt->setString(paramIdx++, origin);
            }

            std::unique_ptr<sql::ResultSet> rs(pstmt->executeQuery());
            json list = json::array();
            while (rs->next()) {
                list.push_back({
                    {"flight_id", rs->getInt("flight_id")},
                    {"flight_number", rs->getString("flight_number")},
                    {"airline_name", rs->getString("airline_name")},
                    {"origin_airport_id", rs->getInt("origin_airport_id")},
                    {"destination_airport_id", rs->getInt("destination_airport_id")},
                    {"origin_iata", rs->getString("origin_iata")},
                    {"origin_city", rs->getString("origin_city")},
                    {"dest_iata", rs->getString("dest_iata")},
                    {"dest_city", rs->getString("dest_city")},
                    {"aircraft_id", rs->getInt("aircraft_id")},
                    {"registration_no", rs->getString("registration_no")},
                    {"aircraft_model", rs->getString("aircraft_model")},
                    {"total_seats", rs->getInt("total_seats")},
                    {"booked_seats", rs->getInt("booked_seats")},
                    {"departure_gate_id", rs->getInt("departure_gate_id")},
                    {"arrival_gate_id", rs->getInt("arrival_gate_id")},
                    {"dep_gate_code", rs->getString("dep_gate_code")},
                    {"arr_gate_code", rs->getString("arr_gate_code")},
                    {"departure_runway_id", rs->getInt("departure_runway_id")},
                    {"arrival_runway_id", rs->getInt("arrival_runway_id")},
                    {"dep_runway_code", rs->getString("dep_runway_code")},
                    {"arr_runway_code", rs->getString("arr_runway_code")},
                    {"scheduled_departure", rs->getString("scheduled_departure")},
                    {"scheduled_arrival", rs->getString("scheduled_arrival")},
                    {"duration_minutes", rs->getInt("duration_minutes")},
                    {"priority_level", rs->getInt("priority_level")},
                    {"base_fare", rs->getDouble("base_fare")},
                    {"status", rs->getString("status")},
                    {"remarks", rs->isNull("remarks") ? "" : rs->getString("remarks")}
                });
            }
            sendJson(res, 200, {{"success", true}, {"data", list}});
        } catch (const std::exception& e) {
            sendJson(res, 500, {{"success", false}, {"error", e.what()}});
        }
    });

    server.Post("/api/flights", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            auto body = json::parse(req.body);
            std::string flightNum = Utils::toUpper(body.value("flight_number", ""));
            std::string airline = body.value("airline_name", "");
            int originId = body.value("origin_airport_id", 0);
            int destId = body.value("destination_airport_id", 0);
            int aircraftId = body.value("aircraft_id", 0);
            int depGateId = body.value("departure_gate_id", 0);
            int arrGateId = body.value("arrival_gate_id", 0);
            int depRunwayId = body.value("departure_runway_id", 0);
            int arrRunwayId = body.value("arrival_runway_id", 0);
            std::string depTime = Utils::normalizeDateTime(body.value("scheduled_departure", ""));
            std::string arrTime = Utils::normalizeDateTime(body.value("scheduled_arrival", ""));
            int priority = body.value("priority_level", 3);
            double baseFare = body.value("base_fare", 150.0);
            std::string status = body.value("status", "SCHEDULED");
            std::string remarks = body.value("remarks", "");

            if (flightNum.empty() || airline.empty() || !Utils::isValidDateTime(depTime) || !Utils::isValidDateTime(arrTime)) {
                sendJson(res, 400, {{"success", false}, {"error", "Flight number, airline, and valid departure/arrival timestamps are required."}});
                return;
            }
            if (arrTime <= depTime) {
                sendJson(res, 400, {{"success", false}, {"error", "Scheduled arrival must be strictly after scheduled departure."}});
                return;
            }

            auto conn = db.getConnection();

            // Run Conflict Detector (Aircraft, Gates, Runways)
            ConflictResult conflict = ConflictDetector::validateFlightSchedule(
                conn.get(), 0, originId, destId, aircraftId,
                depGateId, arrGateId, depRunwayId, arrRunwayId, depTime, arrTime
            );
            if (conflict.hasConflict) {
                sendJson(res, 409, {
                    {"success", false},
                    {"conflict", true},
                    {"conflict_type", conflict.conflictType},
                    {"conflicting_flight", conflict.conflictingFlightNumber},
                    {"error", conflict.message}
                });
                return;
            }

            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                "INSERT INTO flights ("
                "flight_number, airline_name, origin_airport_id, destination_airport_id, "
                "aircraft_id, departure_gate_id, arrival_gate_id, departure_runway_id, arrival_runway_id, "
                "scheduled_departure, scheduled_arrival, duration_minutes, priority_level, base_fare, status, remarks"
                ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, TIMESTAMPDIFF(MINUTE, ?, ?), ?, ?, ?, ?)"
            ));
            pstmt->setString(1, flightNum);
            pstmt->setString(2, airline);
            pstmt->setInt(3, originId);
            pstmt->setInt(4, destId);
            pstmt->setInt(5, aircraftId);
            pstmt->setInt(6, depGateId);
            pstmt->setInt(7, arrGateId);
            pstmt->setInt(8, depRunwayId);
            pstmt->setInt(9, arrRunwayId);
            pstmt->setString(10, depTime);
            pstmt->setString(11, arrTime);
            pstmt->setString(12, depTime);
            pstmt->setString(13, arrTime);
            pstmt->setInt(14, priority);
            pstmt->setDouble(15, baseFare);
            pstmt->setString(16, status);
            pstmt->setString(17, remarks);
            pstmt->executeUpdate();

            sendJson(res, 201, {{"success", true}, {"message", "Flight scheduled with zero resource conflicts."}});
        } catch (const sql::SQLException& e) {
            sendJson(res, 400, {{"success", false}, {"error", std::string("Database error: ") + e.what()}});
        } catch (const std::exception& e) {
            sendJson(res, 500, {{"success", false}, {"error", e.what()}});
        }
    });

    server.Put(R"(/api/flights/(\d+))", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            int flightId = std::stoi(req.matches[1]);
            auto body = json::parse(req.body);
            std::string flightNum = Utils::toUpper(body.value("flight_number", ""));
            std::string airline = body.value("airline_name", "");
            int originId = body.value("origin_airport_id", 0);
            int destId = body.value("destination_airport_id", 0);
            int aircraftId = body.value("aircraft_id", 0);
            int depGateId = body.value("departure_gate_id", 0);
            int arrGateId = body.value("arrival_gate_id", 0);
            int depRunwayId = body.value("departure_runway_id", 0);
            int arrRunwayId = body.value("arrival_runway_id", 0);
            std::string depTime = Utils::normalizeDateTime(body.value("scheduled_departure", ""));
            std::string arrTime = Utils::normalizeDateTime(body.value("scheduled_arrival", ""));
            int priority = body.value("priority_level", 3);
            double baseFare = body.value("base_fare", 150.0);
            std::string status = body.value("status", "SCHEDULED");
            std::string remarks = body.value("remarks", "");

            if (arrTime <= depTime) {
                sendJson(res, 400, {{"success", false}, {"error", "Scheduled arrival must be after scheduled departure."}});
                return;
            }

            auto conn = db.getConnection();
            if (status != "CANCELLED") {
                ConflictResult conflict = ConflictDetector::validateFlightSchedule(
                    conn.get(), flightId, originId, destId, aircraftId,
                    depGateId, arrGateId, depRunwayId, arrRunwayId, depTime, arrTime
                );
                if (conflict.hasConflict) {
                    sendJson(res, 409, {
                        {"success", false},
                        {"conflict", true},
                        {"conflict_type", conflict.conflictType},
                        {"error", conflict.message}
                    });
                    return;
                }
            }

            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                "UPDATE flights SET flight_number=?, airline_name=?, origin_airport_id=?, destination_airport_id=?, "
                "aircraft_id=?, departure_gate_id=?, arrival_gate_id=?, departure_runway_id=?, arrival_runway_id=?, "
                "scheduled_departure=?, scheduled_arrival=?, duration_minutes=TIMESTAMPDIFF(MINUTE, ?, ?), "
                "priority_level=?, base_fare=?, status=?, remarks=? WHERE flight_id=?"
            ));
            pstmt->setString(1, flightNum);
            pstmt->setString(2, airline);
            pstmt->setInt(3, originId);
            pstmt->setInt(4, destId);
            pstmt->setInt(5, aircraftId);
            pstmt->setInt(6, depGateId);
            pstmt->setInt(7, arrGateId);
            pstmt->setInt(8, depRunwayId);
            pstmt->setInt(9, arrRunwayId);
            pstmt->setString(10, depTime);
            pstmt->setString(11, arrTime);
            pstmt->setString(12, depTime);
            pstmt->setString(13, arrTime);
            pstmt->setInt(14, priority);
            pstmt->setDouble(15, baseFare);
            pstmt->setString(16, status);
            pstmt->setString(17, remarks);
            pstmt->setInt(18, flightId);

            if (pstmt->executeUpdate() == 0) {
                sendJson(res, 404, {{"success", false}, {"error", "Flight not found."}});
                return;
            }
            sendJson(res, 200, {{"success", true}, {"message", "Flight updated successfully."}});
        } catch (const std::exception& e) {
            sendJson(res, 400, {{"success", false}, {"error", e.what()}});
        }
    });

    server.Post(R"(/api/flights/(\d+)/cancel)", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            int flightId = std::stoi(req.matches[1]);
            auto conn = db.getConnection();
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                "UPDATE flights SET status = 'CANCELLED', remarks = CONCAT(COALESCE(remarks, ''), ' [Cancelled by Admin]') "
                "WHERE flight_id = ?"
            ));
            pstmt->setInt(1, flightId);
            if (pstmt->executeUpdate() == 0) {
                sendJson(res, 404, {{"success", false}, {"error", "Flight not found."}});
                return;
            }
            sendJson(res, 200, {{"success", true}, {"message", "Flight cancelled. Associated gates, runways, and aircraft slots released."}});
        } catch (const std::exception& e) {
            sendJson(res, 500, {{"success", false}, {"error", e.what()}});
        }
    });

    // Dry-run conflict checker endpoint for Runway & Gate Monitor
    server.Post("/api/conflicts/check", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            auto body = json::parse(req.body);
            auto conn = db.getConnection();
            ConflictResult c = ConflictDetector::validateFlightSchedule(
                conn.get(),
                body.value("exclude_flight_id", 0),
                body.value("origin_airport_id", 0),
                body.value("destination_airport_id", 0),
                body.value("aircraft_id", 0),
                body.value("departure_gate_id", 0),
                body.value("arrival_gate_id", 0),
                body.value("departure_runway_id", 0),
                body.value("arrival_runway_id", 0),
                Utils::normalizeDateTime(body.value("scheduled_departure", "")),
                Utils::normalizeDateTime(body.value("scheduled_arrival", ""))
            );
            sendJson(res, 200, {
                {"success", true},
                {"has_conflict", c.hasConflict},
                {"conflict_type", c.conflictType},
                {"conflicting_flight", c.conflictingFlightNumber},
                {"message", c.message}
            });
        } catch (const std::exception& e) {
            sendJson(res, 400, {{"success", false}, {"error", e.what()}});
        }
    });

    // =========================================================================
    // 5. PASSENGER MANAGEMENT (CRUD)
    // =========================================================================
    server.Get("/api/passengers", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            auto conn = db.getConnection();
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                "SELECT p.*, "
                "(SELECT COUNT(*) FROM bookings b WHERE b.passenger_id = p.passenger_id AND b.booking_status = 'CONFIRMED') AS active_bookings "
                "FROM passengers p ORDER BY p.full_name ASC"
            ));
            std::unique_ptr<sql::ResultSet> rs(pstmt->executeQuery());
            json list = json::array();
            while (rs->next()) {
                list.push_back({
                    {"passenger_id", rs->getInt("passenger_id")},
                    {"full_name", rs->getString("full_name")},
                    {"email", rs->getString("email")},
                    {"phone", rs->getString("phone")},
                    {"passport_number", rs->getString("passport_number")},
                    {"nationality", rs->getString("nationality")},
                    {"date_of_birth", rs->isNull("date_of_birth") ? "" : rs->getString("date_of_birth")},
                    {"active_bookings", rs->getInt("active_bookings")}
                });
            }
            sendJson(res, 200, {{"success", true}, {"data", list}});
        } catch (const std::exception& e) {
            sendJson(res, 500, {{"success", false}, {"error", e.what()}});
        }
    });

    server.Post("/api/passengers", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            auto body = json::parse(req.body);
            std::string name = body.value("full_name", "");
            std::string email = body.value("email", "");
            std::string phone = body.value("phone", "");
            std::string passport = Utils::toUpper(body.value("passport_number", ""));
            std::string nationality = body.value("nationality", "");
            std::string dob = body.value("date_of_birth", "");

            if (name.empty() || !Utils::isValidEmail(email) || phone.empty() || passport.empty() || nationality.empty()) {
                sendJson(res, 400, {{"success", false}, {"error", "Full name, valid email, phone, passport number, and nationality are required."}});
                return;
            }

            auto conn = db.getConnection();
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                "INSERT INTO passengers (full_name, email, phone, passport_number, nationality, date_of_birth) "
                "VALUES (?, ?, ?, ?, ?, ?)"
            ));
            pstmt->setString(1, name);
            pstmt->setString(2, email);
            pstmt->setString(3, phone);
            pstmt->setString(4, passport);
            pstmt->setString(5, nationality);
            if (dob.empty()) pstmt->setNull(6, sql::DataType::DATE);
            else pstmt->setString(6, dob);
            pstmt->executeUpdate();

            sendJson(res, 201, {{"success", true}, {"message", "Passenger registered."}});
        } catch (const sql::SQLException&) {
            sendJson(res, 409, {{"success", false}, {"error", "Email or Passport Number is already registered."}});
        }
    });

    server.Put(R"(/api/passengers/(\d+))", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            int id = std::stoi(req.matches[1]);
            auto body = json::parse(req.body);
            auto conn = db.getConnection();
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                "UPDATE passengers SET full_name=?, email=?, phone=?, passport_number=?, nationality=?, date_of_birth=? "
                "WHERE passenger_id=?"
            ));
            pstmt->setString(1, body.value("full_name", ""));
            pstmt->setString(2, body.value("email", ""));
            pstmt->setString(3, body.value("phone", ""));
            pstmt->setString(4, Utils::toUpper(body.value("passport_number", "")));
            pstmt->setString(5, body.value("nationality", ""));
            std::string dob = body.value("date_of_birth", "");
            if (dob.empty()) pstmt->setNull(6, sql::DataType::DATE);
            else pstmt->setString(6, dob);
            pstmt->setInt(7, id);

            if (pstmt->executeUpdate() == 0) {
                sendJson(res, 404, {{"success", false}, {"error", "Passenger not found."}});
                return;
            }
            sendJson(res, 200, {{"success", true}, {"message", "Passenger updated."}});
        } catch (const std::exception& e) {
            sendJson(res, 400, {{"success", false}, {"error", e.what()}});
        }
    });

    server.Delete(R"(/api/passengers/(\d+))", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            int id = std::stoi(req.matches[1]);
            auto conn = db.getConnection();
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                "DELETE FROM passengers WHERE passenger_id = ?"
            ));
            pstmt->setInt(1, id);
            if (pstmt->executeUpdate() == 0) {
                sendJson(res, 404, {{"success", false}, {"error", "Passenger not found."}});
                return;
            }
            sendJson(res, 200, {{"success", true}, {"message", "Passenger deleted."}});
        } catch (const sql::SQLException&) {
            sendJson(res, 409, {{"success", false}, {"error", "Cannot delete passenger with existing booking records."}});
        }
    });

    // =========================================================================
    // 6. BOOKING MANAGEMENT (Create, Seat Map, View, Cancel, Duplicate Seat Guard)
    // =========================================================================
    server.Get("/api/bookings", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            auto conn = db.getConnection();
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                "SELECT b.*, f.flight_number, f.airline_name, f.scheduled_departure, "
                "o.iata_code AS origin_iata, d.iata_code AS dest_iata, "
                "p.full_name AS passenger_name, p.passport_number "
                "FROM bookings b "
                "JOIN flights f ON b.flight_id = f.flight_id "
                "JOIN airports o ON f.origin_airport_id = o.airport_id "
                "JOIN airports d ON f.destination_airport_id = d.airport_id "
                "JOIN passengers p ON b.passenger_id = p.passenger_id "
                "ORDER BY b.booked_at DESC"
            ));
            std::unique_ptr<sql::ResultSet> rs(pstmt->executeQuery());
            json list = json::array();
            while (rs->next()) {
                list.push_back({
                    {"booking_id", rs->getInt("booking_id")},
                    {"booking_reference", rs->getString("booking_reference")},
                    {"flight_id", rs->getInt("flight_id")},
                    {"flight_number", rs->getString("flight_number")},
                    {"airline_name", rs->getString("airline_name")},
                    {"route", rs->getString("origin_iata") + " -> " + rs->getString("dest_iata")},
                    {"scheduled_departure", rs->getString("scheduled_departure")},
                    {"passenger_id", rs->getInt("passenger_id")},
                    {"passenger_name", rs->getString("passenger_name")},
                    {"passport_number", rs->getString("passport_number")},
                    {"seat_number", rs->getString("seat_number")},
                    {"seat_class", rs->getString("seat_class")},
                    {"fare_paid", rs->getDouble("fare_paid")},
                    {"booking_status", rs->getString("booking_status")},
                    {"booked_at", rs->getString("booked_at")}
                });
            }
            sendJson(res, 200, {{"success", true}, {"data", list}});
        } catch (const std::exception& e) {
            sendJson(res, 500, {{"success", false}, {"error", e.what()}});
        }
    });

    // Returns occupied active seats for a specific flight (for interactive seat map)
    server.Get(R"(/api/flights/(\d+)/seats)", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            int flightId = std::stoi(req.matches[1]);
            auto conn = db.getConnection();
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                "SELECT b.seat_number, b.seat_class, b.booking_reference, p.full_name "
                "FROM bookings b JOIN passengers p ON b.passenger_id = p.passenger_id "
                "WHERE b.flight_id = ? AND b.booking_status = 'CONFIRMED'"
            ));
            pstmt->setInt(1, flightId);
            std::unique_ptr<sql::ResultSet> rs(pstmt->executeQuery());
            json occupied = json::array();
            while (rs->next()) {
                occupied.push_back({
                    {"seat_number", rs->getString("seat_number")},
                    {"seat_class", rs->getString("seat_class")},
                    {"booking_reference", rs->getString("booking_reference")},
                    {"passenger_name", rs->getString("full_name")}
                });
            }
            sendJson(res, 200, {{"success", true}, {"occupied_seats", occupied}});
        } catch (const std::exception& e) {
            sendJson(res, 500, {{"success", false}, {"error", e.what()}});
        }
    });

    server.Post("/api/bookings", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            auto body = json::parse(req.body);
            int flightId = body.value("flight_id", 0);
            int passengerId = body.value("passenger_id", 0);
            std::string seat = Utils::toUpper(body.value("seat_number", ""));
            std::string seatClass = Utils::toUpper(body.value("seat_class", "ECONOMY"));

            if (flightId <= 0 || passengerId <= 0 || !Utils::isValidSeatNumber(seat)) {
                sendJson(res, 400, {{"success", false}, {"error", "Valid flight, passenger, and seat format (e.g., 12A) are required."}});
                return;
            }

            auto conn = db.getConnection();

            // 1. Check flight is active & fetch base fare
            double baseFare = 150.0;
            {
                std::unique_ptr<sql::PreparedStatement> fStmt(conn->prepareStatement(
                    "SELECT status, base_fare FROM flights WHERE flight_id = ?"
                ));
                fStmt->setInt(1, flightId);
                std::unique_ptr<sql::ResultSet> fRes(fStmt->executeQuery());
                if (!fRes->next()) {
                    sendJson(res, 404, {{"success", false}, {"error", "Flight not found."}});
                    return;
                }
                if (fRes->getString("status") == "CANCELLED") {
                    sendJson(res, 400, {{"success", false}, {"error", "Cannot book tickets on a cancelled flight."}});
                    return;
                }
                baseFare = fRes->getDouble("base_fare");
            }

            // 2. Application-level duplicate active seat check (paired with DB unique constraint)
            {
                std::unique_ptr<sql::PreparedStatement> sStmt(conn->prepareStatement(
                    "SELECT booking_reference FROM bookings WHERE flight_id = ? AND seat_number = ? AND booking_status = 'CONFIRMED'"
                ));
                sStmt->setInt(1, flightId);
                sStmt->setString(2, seat);
                std::unique_ptr<sql::ResultSet> sRes(sStmt->executeQuery());
                if (sRes->next()) {
                    sendJson(res, 409, {
                        {"success", false},
                        {"error", "Seat " + seat + " is already booked (Ref: " + sRes->getString("booking_reference") + "). Please select an available seat."}
                    });
                    return;
                }
            }

            double multiplier = (seatClass == "FIRST") ? 3.0 : (seatClass == "BUSINESS" ? 2.0 : 1.0);
            double farePaid = baseFare * multiplier;
            std::string pnr = Utils::generateBookingRef();

            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                "INSERT INTO bookings (booking_reference, flight_id, passenger_id, seat_number, seat_class, fare_paid, booking_status) "
                "VALUES (?, ?, ?, ?, ?, ?, 'CONFIRMED')"
            ));
            pstmt->setString(1, pnr);
            pstmt->setInt(2, flightId);
            pstmt->setInt(3, passengerId);
            pstmt->setString(4, seat);
            pstmt->setString(5, seatClass);
            pstmt->setDouble(6, farePaid);
            pstmt->executeUpdate();

            sendJson(res, 201, {
                {"success", true},
                {"booking_reference", pnr},
                {"fare_paid", farePaid},
                {"message", "Booking confirmed! PNR: " + pnr + " | Seat: " + seat}
            });
        } catch (const sql::SQLException& e) {
            if (e.getErrorCode() == 1062) {
                sendJson(res, 409, {{"success", false}, {"error", "Duplicate active seat booking blocked by database constraint."}});
            } else {
                sendJson(res, 400, {{"success", false}, {"error", e.what()}});
            }
        }
    });

    server.Post(R"(/api/bookings/(\d+)/cancel)", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            int bookingId = std::stoi(req.matches[1]);
            auto conn = db.getConnection();
            std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                "UPDATE bookings SET booking_status = 'CANCELLED', cancelled_at = NOW() "
                "WHERE booking_id = ? AND booking_status = 'CONFIRMED'"
            ));
            pstmt->setInt(1, bookingId);
            if (pstmt->executeUpdate() == 0) {
                sendJson(res, 404, {{"success", false}, {"error", "Active booking not found or already cancelled."}});
                return;
            }
            sendJson(res, 200, {{"success", true}, {"message", "Booking cancelled and seat released for rebooking."}});
        } catch (const std::exception& e) {
            sendJson(res, 500, {{"success", false}, {"error", e.what()}});
        }
    });

    // =========================================================================
    // 7. SCHEDULING ALGORITHMS ENGINE (FCFS, SJF, Priority, Round Robin)
    // =========================================================================
    server.Post("/api/scheduler/run", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            auto body = json::parse(req.body);
            std::string algorithm = Utils::toUpper(body.value("algorithm", "FCFS"));
            int quantum = body.value("quantum", 10);
            std::string source = body.value("source", "DATABASE");

            std::vector<FlightJob> jobs;

            if (source == "DATABASE") {
                // Pull active non-cancelled flights from MySQL and map arrival offsets & runway service times
                auto conn = db.getConnection();
                std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                    "SELECT flight_id, flight_number, airline_name, duration_minutes, priority_level, "
                    "TIMESTAMPDIFF(MINUTE, (SELECT MIN(scheduled_departure) FROM flights WHERE status <> 'CANCELLED'), scheduled_departure) AS arrival_offset "
                    "FROM flights WHERE status <> 'CANCELLED' ORDER BY scheduled_departure ASC LIMIT 12"
                ));
                std::unique_ptr<sql::ResultSet> rs(pstmt->executeQuery());
                while (rs->next()) {
                    FlightJob job;
                    job.flightId = rs->getInt("flight_id");
                    job.flightNumber = rs->getString("flight_number");
                    job.airlineName = rs->getString("airline_name");
                    // Scale arrival offset into realistic ATC slot arrival minutes
                    job.arrivalTime = std::max(0, rs->getInt("arrival_offset") / 15);
                    // Runway / ATC turnaround burst time derived from flight class/duration (10..35 min)
                    int rawDur = rs->getInt("duration_minutes");
                    job.burstTime = std::max(8, std::min(35, rawDur / 15));
                    job.priority = rs->getInt("priority_level");
                    jobs.push_back(job);
                }
            } else if (body.contains("jobs") && body["jobs"].is_array()) {
                for (const auto& item : body["jobs"]) {
                    FlightJob job;
                    job.flightNumber = item.value("flight_number", "FLT-00");
                    job.airlineName = item.value("airline_name", "Custom Carrier");
                    job.arrivalTime = std::max(0, item.value("arrival_time", 0));
                    job.burstTime = std::max(1, item.value("burst_time", 10));
                    job.priority = std::max(1, std::min(4, item.value("priority", 3)));
                    jobs.push_back(job);
                }
            }

            if (jobs.empty()) {
                sendJson(res, 400, {{"success", false}, {"error", "No flights available to schedule."}});
                return;
            }

            ScheduleOutput output;
            if (algorithm == "FCFS") output = FlightScheduler::runFCFS(jobs);
            else if (algorithm == "SJF") output = FlightScheduler::runSJF(jobs);
            else if (algorithm == "PRIORITY") output = FlightScheduler::runPriority(jobs);
            else if (algorithm == "RR" || algorithm == "ROUND_ROBIN") output = FlightScheduler::runRoundRobin(jobs, quantum);
            else {
                sendJson(res, 400, {{"success", false}, {"error", "Unsupported scheduling algorithm: " + algorithm}});
                return;
            }

            json ganttJson = json::array();
            for (const auto& g : output.ganttChart) {
                ganttJson.push_back({
                    {"flight_number", g.flightNumber},
                    {"start_time", g.startTime},
                    {"end_time", g.endTime},
                    {"duration", g.duration}
                });
            }

            json jobsJson = json::array();
            for (const auto& j : output.jobs) {
                jobsJson.push_back({
                    {"flight_number", j.flightNumber},
                    {"airline_name", j.airlineName},
                    {"arrival_time", j.arrivalTime},
                    {"burst_time", j.burstTime},
                    {"priority", j.priority},
                    {"completion_time", j.completionTime},
                    {"turnaround_time", j.turnaroundTime},
                    {"waiting_time", j.waitingTime}
                });
            }

            sendJson(res, 200, {
                {"success", true},
                {"algorithm", output.algorithm},
                {"time_quantum", output.timeQuantum},
                {"gantt_chart", ganttJson},
                {"results", jobsJson},
                {"avg_waiting_time", output.avgWaitingTime},
                {"avg_turnaround_time", output.avgTurnaroundTime},
                {"utilization_percent", output.utilizationPercent}
            });
        } catch (const std::exception& e) {
            sendJson(res, 500, {{"success", false}, {"error", e.what()}});
        }
    });

    // =========================================================================
    // 8. REPORTS & EXECUTIVE ANALYTICS
    // =========================================================================
    server.Get("/api/reports/summary", [&db, &sessions](const httplib::Request& req, httplib::Response& res) {
        AdminSession admin;
        if (!requireAuth(req, res, sessions, admin)) return;
        try {
            auto conn = db.getConnection();
            json report;

            auto scalarInt = [&](const std::string& query) -> int {
                std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(query));
                std::unique_ptr<sql::ResultSet> rs(pstmt->executeQuery());
                return rs->next() ? rs->getInt(1) : 0;
            };

            auto scalarDouble = [&](const std::string& query) -> double {
                std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(query));
                std::unique_ptr<sql::ResultSet> rs(pstmt->executeQuery());
                return rs->next() ? rs->getDouble(1) : 0.0;
            };

            report["total_airports"] = scalarInt("SELECT COUNT(*) FROM airports");
            report["total_aircraft"] = scalarInt("SELECT COUNT(*) FROM aircraft");
            report["available_aircraft"] = scalarInt("SELECT COUNT(*) FROM aircraft WHERE status = 'AVAILABLE'");
            report["total_flights"] = scalarInt("SELECT COUNT(*) FROM flights");
            report["cancelled_flights"] = scalarInt("SELECT COUNT(*) FROM flights WHERE status = 'CANCELLED'");
            report["total_passengers"] = scalarInt("SELECT COUNT(*) FROM passengers");
            report["confirmed_bookings"] = scalarInt("SELECT COUNT(*) FROM bookings WHERE booking_status = 'CONFIRMED'");
            report["cancelled_bookings"] = scalarInt("SELECT COUNT(*) FROM bookings WHERE booking_status = 'CANCELLED'");
            report["total_revenue"] = scalarDouble("SELECT COALESCE(SUM(fare_paid), 0) FROM bookings WHERE booking_status = 'CONFIRMED'");

            // Flight status breakdown
            json statusBreakdown = json::array();
            {
                std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                    "SELECT status, COUNT(*) AS count FROM flights GROUP BY status ORDER BY count DESC"
                ));
                std::unique_ptr<sql::ResultSet> rs(pstmt->executeQuery());
                while (rs->next()) {
                    statusBreakdown.push_back({
                        {"status", rs->getString("status")},
                        {"count", rs->getInt("count")}
                    });
                }
            }
            report["flight_status_summary"] = statusBreakdown;

            // Aircraft status breakdown
            json aircraftBreakdown = json::array();
            {
                std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                    "SELECT status, COUNT(*) AS count FROM aircraft GROUP BY status ORDER BY count DESC"
                ));
                std::unique_ptr<sql::ResultSet> rs(pstmt->executeQuery());
                while (rs->next()) {
                    aircraftBreakdown.push_back({
                        {"status", rs->getString("status")},
                        {"count", rs->getInt("count")}
                    });
                }
            }
            report["aircraft_status_summary"] = aircraftBreakdown;

            sendJson(res, 200, {{"success", true}, {"report", report}});
        } catch (const std::exception& e) {
            sendJson(res, 500, {{"success", false}, {"error", e.what()}});
        }
    });
}
