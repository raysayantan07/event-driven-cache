#include "SnoopBus.hpp"
#include "Cache.hpp"
#include <cstdint>

SnoopBus::SnoopBus(EventSimulator& sim) : sim(sim) {}

void SnoopBus::register_cache(ICache* cache){
    caches.push_back(cache);
}

void SnoopBus::broadcast_snoop(ICache* source, bool is_write, uint64_t addr, int snoop_lt){
    for (auto* cache : caches){
        if (cache == source) continue;
        sim.schedule(sim.now() + snoop_lt, [=](){
            bool snoop_success = false;
            if(is_write) snoop_success = cache->snoop_write(addr);
            else         snoop_success = cache->snoop_read(addr);
            std::cout << "@ " << sim.now() 
                      << " Cache_" << source->name() 
                      << " snooped Cache_" << cache->name() 
                      << " for addr(" << addr << ") --> ";
            if (snoop_success) std::cout << "SNOOP SUCCESS!" << std::endl;
            else               std::cout << "SNOOP FAILED!" << std::endl;
            if (is_write) source->wr_miss_callback(snoop_success, addr);
            else          source->rd_miss_callback(snoop_success, addr);
            });
    }
}

