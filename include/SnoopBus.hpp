#pragma once
#include <vector>
#include <iostream>
#include <string>
#include <cstdint>
#include "EventSimulator.hpp"

class ICache;

// ------------------- Snoop bus ---------------------
struct SnoopBus {
    EventSimulator& sim;
    std::vector<ICache*> caches;

public:

    explicit SnoopBus(EventSimulator& sim);
    void register_cache(ICache* cache);
    void broadcast_snoop(ICache* source, bool is_write, uint64_t addr, int snoop_lt);
};


