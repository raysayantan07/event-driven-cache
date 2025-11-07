#include "Cache.hpp"
#include "Coherence.hpp"
#include "Eviction.hpp"
#include "SnoopBus.hpp"
#include "EventSimulator.hpp"
#include "Logger.hpp"

int main() {
    EventSimulator sim;
    SnoopBus bus(sim);
    ConsoleLogger logger;

    // Example: construct a cache and pass logger (replace CoherencePolicy/Eviction with actual types)
    // Cache<MyCoherencePolicy, MyEvictionPolicy> l1("L1", 64, 256, 8, 1<<20, 1, 10, 1, 10, sim, bus, logger);

    size_t blk_size = 64;
    size_t num_sets = 16;
    size_t assoc    = 4;
    size_t mm_size  = 65536;
    int rd_hit_lt   = 5;
    int rd_miss_lt  = 15;
    int wr_hit_lt   = 5;
    int wr_miss_lt  = 15;

    Cache<MESICoherence, LRUEviction> L1("L1A", blk_size, num_sets, assoc, mm_size, rd_hit_lt, rd_miss_lt, wr_hit_lt, wr_miss_lt, sim, bus, logger); 

    sim.schedule(0, [&](){ L1.read(0x1000); });
    sim.schedule(1, [&](){ L1.read(0x2000); });
    sim.schedule(2, [&](){ L1.read(0x3000); });
    sim.schedule(3, [&](){ L1.read(0x4000); });
    sim.schedule(4, [&](){ L1.read(0x5000); });
    sim.schedule(10, [&](){ L1.read(0x1000); });
    sim.schedule(50, [&](){ L1.read(0x1000); });
    sim.schedule(51, [&](){ L1.read(0x2000); });
    sim.schedule(100, [&](){ L1.write(0x2000); });
    sim.schedule(120, [&](){ L1.write(0x2010); });
    sim.schedule(130, [&](){ L1.write(0x6010); });
    sim.schedule(140, [&](){ L1.read(0x6010); });
    sim.schedule(150, [&](){ L1.read(0x6010); });
    sim.run_sim();
    return 0;
}
