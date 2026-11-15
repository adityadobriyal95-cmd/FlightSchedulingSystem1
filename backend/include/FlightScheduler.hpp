#pragma once
#include "Models.hpp"
#include <vector>
#include <string>

class FlightScheduler {
public:
    static ScheduleOutput runFCFS(std::vector<FlightJob> jobs);
    static ScheduleOutput runSJF(std::vector<FlightJob> jobs);
    static ScheduleOutput runPriority(std::vector<FlightJob> jobs);
    static ScheduleOutput runRoundRobin(std::vector<FlightJob> jobs, int timeQuantum);
};
