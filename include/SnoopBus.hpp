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
    uint64_t SNOOP_LT = 2;

public:

    explicit SnoopBus(EventSimulator& sim);
    void register_cache(ICache* cache);
    uint64_t broadcast_snoop(ICache* source, bool is_write, uint64_t addr);
};


