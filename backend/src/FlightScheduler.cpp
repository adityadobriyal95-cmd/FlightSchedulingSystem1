#include "FlightScheduler.hpp"
#include <algorithm>
#include <queue>
#include <numeric>

namespace {
void finalizeMetrics(ScheduleOutput& out, int totalIdleTime) {
    if (out.jobs.empty()) return;
    double sumWT = 0.0;
    double sumTAT = 0.0;
    int maxCT = 0;

    for (auto& j : out.jobs) {
        j.turnaroundTime = j.completionTime - j.arrivalTime;
        j.waitingTime = j.turnaroundTime - j.burstTime;
        if (j.waitingTime < 0) j.waitingTime = 0;
        sumWT += j.waitingTime;
        sumTAT += j.turnaroundTime;
        maxCT = std::max(maxCT, j.completionTime);
    }

    out.avgWaitingTime = sumWT / static_cast<double>(out.jobs.size());
    out.avgTurnaroundTime = sumTAT / static_cast<double>(out.jobs.size());
    if (maxCT > 0) {
        int busy = maxCT - totalIdleTime;
        out.utilizationPercent = (static_cast<double>(busy) / static_cast<double>(maxCT)) * 100.0;
    }
}
}

ScheduleOutput FlightScheduler::runFCFS(std::vector<FlightJob> jobs) {
    ScheduleOutput out;
    out.algorithm = "FCFS (First-Come, First-Served)";
    if (jobs.empty()) return out;

    std::stable_sort(jobs.begin(), jobs.end(), [](const FlightJob& a, const FlightJob& b) {
        return a.arrivalTime < b.arrivalTime;
    });

    int currentTime = 0;
    int idleTime = 0;

    for (const auto& job : jobs) {
        if (currentTime < job.arrivalTime) {
            idleTime += (job.arrivalTime - currentTime);
            currentTime = job.arrivalTime;
        }
        int start = currentTime;
        int end = start + job.burstTime;
        currentTime = end;

        out.ganttChart.push_back({job.flightNumber, start, end, job.burstTime});
        out.jobs.push_back({
            job.flightNumber, job.airlineName,
            job.arrivalTime, job.burstTime, job.priority,
            end, 0, 0
        });
    }

    finalizeMetrics(out, idleTime);
    return out;
}

ScheduleOutput FlightScheduler::runSJF(std::vector<FlightJob> jobs) {
    ScheduleOutput out;
    out.algorithm = "SJF (Shortest Job First - Non-Preemptive)";
    if (jobs.empty()) return out;

    size_t n = jobs.size();
    std::vector<bool> completed(n, false);
    size_t completedCount = 0;
    int currentTime = 0;
    int idleTime = 0;

    while (completedCount < n) {
        int selectedIdx = -1;
        int minBurst = 1e9;

        for (size_t i = 0; i < n; ++i) {
            if (!completed[i] && jobs[i].arrivalTime <= currentTime) {
                if (jobs[i].burstTime < minBurst ||
                   (jobs[i].burstTime == minBurst && jobs[i].arrivalTime < jobs[selectedIdx].arrivalTime)) {
                    minBurst = jobs[i].burstTime;
                    selectedIdx = static_cast<int>(i);
                }
            }
        }

        if (selectedIdx == -1) {
            // Advance clock to earliest remaining arrival
            int nextArrival = 1e9;
            for (size_t i = 0; i < n; ++i) {
                if (!completed[i]) nextArrival = std::min(nextArrival, jobs[i].arrivalTime);
            }
            idleTime += (nextArrival - currentTime);
            currentTime = nextArrival;
            continue;
        }

        const auto& job = jobs[selectedIdx];
        int start = currentTime;
        int end = start + job.burstTime;
        currentTime = end;
        completed[selectedIdx] = true;
        completedCount++;

        out.ganttChart.push_back({job.flightNumber, start, end, job.burstTime});
        out.jobs.push_back({
            job.flightNumber, job.airlineName,
            job.arrivalTime, job.burstTime, job.priority,
            end, 0, 0
        });
    }

    finalizeMetrics(out, idleTime);
    return out;
}

ScheduleOutput FlightScheduler::runPriority(std::vector<FlightJob> jobs) {
    ScheduleOutput out;
    out.algorithm = "Priority Scheduling (1=Emergency/VIP Highest, 4=Cargo Lowest)";
    if (jobs.empty()) return out;

    size_t n = jobs.size();
    std::vector<bool> completed(n, false);
    size_t completedCount = 0;
    int currentTime = 0;
    int idleTime = 0;

    while (completedCount < n) {
        int selectedIdx = -1;
        int bestPriority = 1e9;

        for (size_t i = 0; i < n; ++i) {
            if (!completed[i] && jobs[i].arrivalTime <= currentTime) {
                if (jobs[i].priority < bestPriority ||
                   (jobs[i].priority == bestPriority && jobs[i].arrivalTime < jobs[selectedIdx].arrivalTime)) {
                    bestPriority = jobs[i].priority;
                    selectedIdx = static_cast<int>(i);
                }
            }
        }

        if (selectedIdx == -1) {
            int nextArrival = 1e9;
            for (size_t i = 0; i < n; ++i) {
                if (!completed[i]) nextArrival = std::min(nextArrival, jobs[i].arrivalTime);
            }
            idleTime += (nextArrival - currentTime);
            currentTime = nextArrival;
            continue;
        }

        const auto& job = jobs[selectedIdx];
        int start = currentTime;
        int end = start + job.burstTime;
        currentTime = end;
        completed[selectedIdx] = true;
        completedCount++;

        out.ganttChart.push_back({job.flightNumber, start, end, job.burstTime});
        out.jobs.push_back({
            job.flightNumber, job.airlineName,
            job.arrivalTime, job.burstTime, job.priority,
            end, 0, 0
        });
    }

    finalizeMetrics(out, idleTime);
    return out;
}

ScheduleOutput FlightScheduler::runRoundRobin(std::vector<FlightJob> jobs, int timeQuantum) {
    ScheduleOutput out;
    if (timeQuantum <= 0) timeQuantum = 5;
    out.algorithm = "Round Robin (Quantum = " + std::to_string(timeQuantum) + " min)";
    out.timeQuantum = timeQuantum;
    if (jobs.empty()) return out;

    std::stable_sort(jobs.begin(), jobs.end(), [](const FlightJob& a, const FlightJob& b) {
        return a.arrivalTime < b.arrivalTime;
    });

    size_t n = jobs.size();
    std::vector<int> remaining(n);
    std::vector<int> completion(n, 0);
    std::vector<bool> inQueue(n, false);
    for (size_t i = 0; i < n; ++i) remaining[i] = jobs[i].burstTime;

    std::queue<size_t> readyQueue;
    int currentTime = 0;
    int idleTime = 0;
    size_t completedCount = 0;
    size_t nextArrivalIdx = 0;

    auto enqueueArrivals = [&]() {
        while (nextArrivalIdx < n && jobs[nextArrivalIdx].arrivalTime <= currentTime) {
            if (!inQueue[nextArrivalIdx] && remaining[nextArrivalIdx] > 0) {
                readyQueue.push(nextArrivalIdx);
                inQueue[nextArrivalIdx] = true;
            }
            nextArrivalIdx++;
        }
    };

    enqueueArrivals();

    while (completedCount < n) {
        if (readyQueue.empty()) {
            if (nextArrivalIdx < n) {
                idleTime += (jobs[nextArrivalIdx].arrivalTime - currentTime);
                currentTime = jobs[nextArrivalIdx].arrivalTime;
                enqueueArrivals();
            }
            continue;
        }

        size_t idx = readyQueue.front();
        readyQueue.pop();
        inQueue[idx] = false;

        int slice = std::min(timeQuantum, remaining[idx]);
        int start = currentTime;
        int end = start + slice;
        currentTime = end;
        remaining[idx] -= slice;

        // Merge consecutive slices of the exact same flight in Gantt chart
        if (!out.ganttChart.empty() && out.ganttChart.back().flightNumber == jobs[idx].flightNumber &&
            out.ganttChart.back().endTime == start) {
            out.ganttChart.back().endTime = end;
            out.ganttChart.back().duration += slice;
        } else {
            out.ganttChart.push_back({jobs[idx].flightNumber, start, end, slice});
        }

        // Enqueue newly arrived flights BEFORE re-queueing current flight
        enqueueArrivals();

        if (remaining[idx] == 0) {
            completion[idx] = end;
            completedCount++;
        } else {
            readyQueue.push(idx);
            inQueue[idx] = true;
        }
    }

    for (size_t i = 0; i < n; ++i) {
        out.jobs.push_back({
            jobs[i].flightNumber, jobs[i].airlineName,
            jobs[i].arrivalTime, jobs[i].burstTime, jobs[i].priority,
            completion[i], 0, 0
        });
    }

    finalizeMetrics(out, idleTime);
    return out;
}
