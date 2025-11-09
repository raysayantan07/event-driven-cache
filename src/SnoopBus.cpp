#include "SnoopBus.hpp"
#include "Cache.hpp"
#include <cstdint>

SnoopBus::SnoopBus(EventSimulator& sim, Logger& logger) : sim(sim), logger(logger) {}

void SnoopBus::register_cache(ICache* cache){
    caches.push_back(cache);
}

void SnoopBus::broadcast_snoop(ICache* source, bool is_write, bool is_miss, uint64_t addr, int snoop_lt){
    for (auto* cache : caches){
        if (cache == source) continue;
        sim.schedule(sim.now() + snoop_lt, [=](){
            bool snoop_success = false;
            if(is_write) snoop_success = cache->snoop_write(addr);
            else         snoop_success = cache->snoop_read(addr);
            std::string snoop_result   = snoop_success ? "SNOOP SUCCESS!" : "SNOOP FAILED!"; 
            logger.log(sim.now(), "Cache_" + source->name() + " :: SNOOPED " +
                                  "Cache_" + cache->name() + " for addr(" +
                                  to_string(addr) + ") --> " + snoop_result);
            if (is_write && is_miss) source->wr_miss_callback(snoop_success, addr);
            else if (!is_write)      source->rd_miss_callback(snoop_success, addr);
            });
    }
}

