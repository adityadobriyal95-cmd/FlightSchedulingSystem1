#include "ConflictDetector.hpp"
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <memory>

ConflictResult ConflictDetector::validateFlightSchedule(
    sql::Connection* conn,
    int excludeFlightId,
    int originAirportId,
    int destinationAirportId,
    int aircraftId,
    int departureGateId,
    int arrivalGateId,
    int departureRunwayId,
    int arrivalRunwayId,
    const std::string& scheduledDeparture,
    const std::string& scheduledArrival
) {
    if (originAirportId == destinationAirportId) {
        return {true, "VALIDATION", "Origin and destination airports must be different.", ""};
    }

    // 1. Verify Aircraft exists and is not RETIRED or in MAINTENANCE
    {
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT registration_no, status FROM aircraft WHERE aircraft_id = ?"
        ));
        pstmt->setInt(1, aircraftId);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (!res->next()) {
            return {true, "VALIDATION", "Selected aircraft does not exist.", ""};
        }
        std::string status = res->getString("status");
        std::string reg = res->getString("registration_no");
        if (status == "MAINTENANCE" || status == "RETIRED") {
            return {true, "AIRCRAFT",
                    "Aircraft " + reg + " is currently marked as " + status + " and cannot be scheduled.",
                    ""};
        }
    }

    // 2. Verify Gate & Runway ownership against Origin and Destination airports
    {
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT airport_id, gate_code, status FROM gates WHERE gate_id = ?"
        ));
        pstmt->setInt(1, departureGateId);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (!res->next() || res->getInt("airport_id") != originAirportId) {
            return {true, "VALIDATION", "Departure gate does not belong to the selected origin airport.", ""};
        }
        if (res->getString("status") != "AVAILABLE") {
            return {true, "DEPARTURE_GATE", "Departure gate " + res->getString("gate_code") + " is under maintenance/closed.", ""};
        }

        pstmt->setInt(1, arrivalGateId);
        res.reset(pstmt->executeQuery());
        if (!res->next() || res->getInt("airport_id") != destinationAirportId) {
            return {true, "VALIDATION", "Arrival gate does not belong to the selected destination airport.", ""};
        }
        if (res->getString("status") != "AVAILABLE") {
            return {true, "ARRIVAL_GATE", "Arrival gate " + res->getString("gate_code") + " is under maintenance/closed.", ""};
        }
    }

    {
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT airport_id, runway_code, status FROM runways WHERE runway_id = ?"
        ));
        pstmt->setInt(1, departureRunwayId);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (!res->next() || res->getInt("airport_id") != originAirportId) {
            return {true, "VALIDATION", "Departure runway does not belong to the selected origin airport.", ""};
        }
        if (res->getString("status") != "ACTIVE") {
            return {true, "DEPARTURE_RUNWAY", "Departure runway " + res->getString("runway_code") + " is inactive.", ""};
        }

        pstmt->setInt(1, arrivalRunwayId);
        res.reset(pstmt->executeQuery());
        if (!res->next() || res->getInt("airport_id") != destinationAirportId) {
            return {true, "VALIDATION", "Arrival runway does not belong to the selected destination airport.", ""};
        }
        if (res->getString("status") != "ACTIVE") {
            return {true, "ARRIVAL_RUNWAY", "Arrival runway " + res->getString("runway_code") + " is inactive.", ""};
        }
    }

    // 3. Aircraft Overlap Check (including 30-minute turnaround buffer before & after)
    {
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT flight_number, scheduled_departure, scheduled_arrival FROM flights "
            "WHERE aircraft_id = ? AND flight_id <> ? AND status <> 'CANCELLED' "
            "AND DATE_SUB(scheduled_departure, INTERVAL 30 MINUTE) < ? "
            "AND DATE_ADD(scheduled_arrival, INTERVAL 30 MINUTE) > ? LIMIT 1"
        ));
        pstmt->setInt(1, aircraftId);
        pstmt->setInt(2, excludeFlightId);
        pstmt->setString(3, scheduledArrival);
        pstmt->setString(4, scheduledDeparture);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res->next()) {
            std::string fn = res->getString("flight_number");
            return {true, "AIRCRAFT",
                    "Aircraft conflict with flight " + fn + " (" +
                    res->getString("scheduled_departure") + " to " +
                    res->getString("scheduled_arrival") + ", including 30m turnaround buffer).",
                    fn};
        }
    }

    // 4. Departure Gate Overlap Check (gate occupied [dep - 45m, dep + 15m])
    {
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT flight_number, scheduled_departure FROM flights "
            "WHERE flight_id <> ? AND status <> 'CANCELLED' AND ("
            "  (departure_gate_id = ? AND ABS(TIMESTAMPDIFF(MINUTE, scheduled_departure, ?)) < 45) OR "
            "  (arrival_gate_id = ? AND ABS(TIMESTAMPDIFF(MINUTE, scheduled_arrival, ?)) < 45)"
            ") LIMIT 1"
        ));
        pstmt->setInt(1, excludeFlightId);
        pstmt->setInt(2, departureGateId);
        pstmt->setString(3, scheduledDeparture);
        pstmt->setInt(4, departureGateId);
        pstmt->setString(5, scheduledDeparture);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res->next()) {
            std::string fn = res->getString("flight_number");
            return {true, "DEPARTURE_GATE",
                    "Departure gate conflict with flight " + fn + " within 45-minute gate turnaround window.",
                    fn};
        }
    }

    // 5. Arrival Gate Overlap Check (gate occupied [arr - 15m, arr + 45m])
    {
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT flight_number FROM flights "
            "WHERE flight_id <> ? AND status <> 'CANCELLED' AND ("
            "  (arrival_gate_id = ? AND ABS(TIMESTAMPDIFF(MINUTE, scheduled_arrival, ?)) < 45) OR "
            "  (departure_gate_id = ? AND ABS(TIMESTAMPDIFF(MINUTE, scheduled_departure, ?)) < 45)"
            ") LIMIT 1"
        ));
        pstmt->setInt(1, excludeFlightId);
        pstmt->setInt(2, arrivalGateId);
        pstmt->setString(3, scheduledArrival);
        pstmt->setInt(4, arrivalGateId);
        pstmt->setString(5, scheduledArrival);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res->next()) {
            std::string fn = res->getString("flight_number");
            return {true, "ARRIVAL_GATE",
                    "Arrival gate conflict with flight " + fn + " within 45-minute gate turnaround window.",
                    fn};
        }
    }

    // 6. Departure Runway Overlap Check (15-minute wake turbulence & ATC separation buffer)
    {
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT flight_number FROM flights "
            "WHERE flight_id <> ? AND status <> 'CANCELLED' AND ("
            "  (departure_runway_id = ? AND ABS(TIMESTAMPDIFF(MINUTE, scheduled_departure, ?)) < 15) OR "
            "  (arrival_runway_id = ? AND ABS(TIMESTAMPDIFF(MINUTE, scheduled_arrival, ?)) < 15)"
            ") LIMIT 1"
        ));
        pstmt->setInt(1, excludeFlightId);
        pstmt->setInt(2, departureRunwayId);
        pstmt->setString(3, scheduledDeparture);
        pstmt->setInt(4, departureRunwayId);
        pstmt->setString(5, scheduledDeparture);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res->next()) {
            std::string fn = res->getString("flight_number");
            return {true, "DEPARTURE_RUNWAY",
                    "Departure runway conflict with flight " + fn + " (minimum 15-minute ATC runway separation required).",
                    fn};
        }
    }

    // 7. Arrival Runway Overlap Check (15-minute wake turbulence & ATC separation buffer)
    {
        std::unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
            "SELECT flight_number FROM flights "
            "WHERE flight_id <> ? AND status <> 'CANCELLED' AND ("
            "  (arrival_runway_id = ? AND ABS(TIMESTAMPDIFF(MINUTE, scheduled_arrival, ?)) < 15) OR "
            "  (departure_runway_id = ? AND ABS(TIMESTAMPDIFF(MINUTE, scheduled_departure, ?)) < 15)"
            ") LIMIT 1"
        ));
        pstmt->setInt(1, excludeFlightId);
        pstmt->setInt(2, arrivalRunwayId);
        pstmt->setString(3, scheduledArrival);
        pstmt->setInt(4, arrivalRunwayId);
        pstmt->setString(5, scheduledArrival);
        std::unique_ptr<sql::ResultSet> res(pstmt->executeQuery());
        if (res->next()) {
            std::string fn = res->getString("flight_number");
            return {true, "ARRIVAL_RUNWAY",
                    "Arrival runway conflict with flight " + fn + " (minimum 15-minute ATC runway separation required).",
                    fn};
        }
    }

    return {false, "", "No scheduling conflicts detected.", ""};
}
