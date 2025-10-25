#include "SnoopBus.hpp"
#include "Cache.hpp"
#include <cstdint>

SnoopBus::SnoopBus(EventSimulator& sim) : sim(sim) {}

void SnoopBus::register_cache(ICache* cache){
    caches.push_back(cache);
}

uint64_t SnoopBus::broadcast_snoop(ICache* source, bool is_write, uint64_t addr){
    for (auto* cache : caches){
        if (cache == source) continue;
        sim.schedule(SNOOP_LT, [=](){
            if(is_write) cache->snoop_write(addr);
            else cache->snoop_read(addr);
            std::cout << "@ " << sim.now() 
                      << " Cache_" << source->name() 
                      << " snooped Cache_" << cache->name() 
                      << " for addr(" << addr << ")" << std::endl;
                });
    }
    return SNOOP_LT;
}

