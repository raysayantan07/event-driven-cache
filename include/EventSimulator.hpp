#pragma once
#include <functional>
#include <queue>
#include <vector>
#include <cstdint>

struct Event {
    uint64_t time;
    std::function<void()> action;
    bool operator<(const Event& other) const { return time > other.time; }
};


class EventSimulator {
    uint64_t currentTime = 0;
    std::priority_queue<Event> event_q; 
public:
    void schedule(uint64_t time, std::function<void()> action);
    void run_sim();
    uint64_t now();
};

