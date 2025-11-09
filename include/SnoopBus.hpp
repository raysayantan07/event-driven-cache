#pragma once
#include <vector>
#include <iostream>
#include <string>
#include <cstdint>
#include "EventSimulator.hpp"
#include "Logger.hpp"

class ICache;

// ------------------- Snoop bus ---------------------
struct SnoopBus {
    EventSimulator& sim;
    Logger& logger;
    std::vector<ICache*> caches;

public:

    explicit SnoopBus(EventSimulator& sim, Logger& logger);
    void register_cache(ICache* cache);
    void broadcast_snoop(ICache* source, bool is_write, bool is_miss, uint64_t addr, int snoop_lt);
};


