#include "Cache.hpp"
#include "Coherence.hpp"
#include "Eviction.hpp"
#include "SnoopBus.hpp"
#include "EventSimulator.hpp"

int main() {
    EventSimulator sim;
    SnoopBus bus(sim);


    size_t blk_size = 64;
    size_t num_sets = 16;
    size_t assoc    = 4;
    size_t mm_size  = 65536;
    int rd_hit_lt   = 5;
    int rd_miss_lt  = 15;
    int wr_hit_lt   = 5;
    int wr_miss_lt  = 15;

    Cache<MESICoherence, LRUEviction> L1("L1A", blk_size, num_sets, assoc, mm_size, rd_hit_lt, rd_miss_lt, wr_hit_lt, wr_miss_lt, sim, bus); 

    sim.schedule(0, [&](){ L1.read(0x1000); });
    sim.schedule(10, [&](){ L1.read(0x1000); });
    sim.schedule(50, [&](){ L1.read(0x1000); });
    sim.run_sim();
    return 0;
}
