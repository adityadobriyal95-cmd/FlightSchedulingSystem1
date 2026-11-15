#pragma once
#include "Models.hpp"
#include <cppconn/connection.h>
#include <string>

class ConflictDetector {
public:
    // Validates all scheduling constraints before inserting or updating a flight:
    // 1. Airport & resource ownership checks (gates/runways must belong to origin/dest airports)
    // 2. Aircraft availability and non-overlapping flight + turnaround buffer (30 min)
    // 3. Departure & arrival gate non-overlapping occupancy window (45 min gate turnaround)
    // 4. Departure & arrival runway wake-turbulence separation window (15 min safety buffer)
    static ConflictResult validateFlightSchedule(
        sql::Connection* conn,
        int excludeFlightId, // 0 when creating a new flight, >0 when editing existing flight
        int originAirportId,
        int destinationAirportId,
        int aircraftId,
        int departureGateId,
        int arrivalGateId,
        int departureRunwayId,
        int arrivalRunwayId,
        const std::string& scheduledDeparture,
        const std::string& scheduledArrival
    );
};
