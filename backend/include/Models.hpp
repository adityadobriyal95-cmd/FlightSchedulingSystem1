#pragma once
#include <string>
#include <vector>

struct ConflictResult {
    bool hasConflict = false;
    std::string conflictType; // AIRCRAFT, DEPARTURE_GATE, ARRIVAL_GATE, DEPARTURE_RUNWAY, ARRIVAL_RUNWAY, VALIDATION
    std::string message;
    std::string conflictingFlightNumber;
};

struct FlightJob {
    int flightId = 0;
    std::string flightNumber;
    std::string airlineName;
    int arrivalTime = 0;   // Minutes relative to schedule epoch
    int burstTime = 10;    // Runway/processing time in minutes
    int priority = 3;      // 1 = Highest (Emergency/VIP), 4 = Lowest (Cargo)
};

struct GanttSlice {
    std::string flightNumber;
    int startTime = 0;
    int endTime = 0;
    int duration = 0;
};

struct ScheduledJobResult {
    std::string flightNumber;
    std::string airlineName;
    int arrivalTime = 0;
    int burstTime = 0;
    int priority = 3;
    int completionTime = 0;
    int turnaroundTime = 0;
    int waitingTime = 0;
};

struct ScheduleOutput {
    std::string algorithm;
    int timeQuantum = 0;
    std::vector<GanttSlice> ganttChart;
    std::vector<ScheduledJobResult> jobs;
    double avgWaitingTime = 0.0;
    double avgTurnaroundTime = 0.0;
    double utilizationPercent = 0.0;
};
