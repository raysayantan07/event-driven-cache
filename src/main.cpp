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

    size_t blk_size = 64;
    size_t num_sets = 16;
    size_t assoc    = 4;
    size_t mm_size  = 65536;
    int rd_hit_lt   = 5;
    int rd_miss_lt  = 15;
    int wr_hit_lt   = 5;
    int wr_miss_lt  = 15;
    int snoop_lt    = 2;
    int snoop_hit_lt= 10; 

    Cache<MESICoherence, LRUEviction> L1A("L1A", blk_size, num_sets, assoc, mm_size, rd_hit_lt, rd_miss_lt, wr_hit_lt, wr_miss_lt, snoop_lt, snoop_hit_lt, sim, bus, logger); 

    // Single cache read/write testing
    /*
    sim.schedule(0, [&](){ L1A.read(0x1000); });
    sim.schedule(1, [&](){ L1A.read(0x2000); });
    sim.schedule(2, [&](){ L1A.read(0x3000); });
    sim.schedule(3, [&](){ L1A.read(0x4000); });
    sim.schedule(4, [&](){ L1A.read(0x5000); });
    sim.schedule(10, [&](){ L1A.read(0x1000); });
    sim.schedule(50, [&](){ L1A.read(0x1000); });
    sim.schedule(51, [&](){ L1A.read(0x2000); });
    sim.schedule(100, [&](){ L1A.write(0x2000); });
    sim.schedule(120, [&](){ L1A.write(0x2010); });
    sim.schedule(130, [&](){ L1A.write(0x6010); });
    sim.schedule(140, [&](){ L1A.read(0x6010); });
    sim.schedule(150, [&](){ L1A.read(0x6010); });
    */

    // Dual Cache coherence testing 
    Cache<MESICoherence, LRUEviction> L1B("L1B", blk_size, num_sets, assoc, mm_size, rd_hit_lt, rd_miss_lt, wr_hit_lt, wr_miss_lt, snoop_lt, snoop_hit_lt, sim, bus, logger); 
    sim.schedule(0, [&](){ L1A.read(0x1000); });
    sim.schedule(1, [&](){ L1B.read(0x1000); });
    sim.schedule(1, [&](){ L1A.read(0x1000); });
    sim.schedule(20, [&](){ L1A.read(0x1000); });
    sim.schedule(25, [&](){ L1B.read(0x1000); });
    sim.schedule(50, [&](){ L1A.read(0x2000); });
    sim.schedule(70, [&](){ L1B.read(0x2000); });


    sim.run_sim();
    return 0;
}
