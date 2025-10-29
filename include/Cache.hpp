#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <iostream>
#include "EventSimulator.hpp"
#include "Coherence.hpp"
#include "Eviction.hpp"
#include "SnoopBus.hpp"
#include "Logger.hpp"
using namespace std;

// -------------------- Base cache ----------------------
class ICache {
public:
    virtual void snoop_read(uint64_t addr) = 0;
    virtual void snoop_write(uint64_t addr) = 0;
    virtual void read(uint64_t addr) = 0;
    virtual void write(uint64_t addr) = 0;
    virtual std::string name() const = 0;
    virtual ~ICache() = default;
};



// -------------------- CacheLine -----------------------
template <typename CoherencePolicy>
struct Line {
    using State = typename CoherencePolicy::StateType;
    uint64_t tag = 0;
    bool valid   = false;
    //uint8_t data[64];  // not needed in PERF MODEL
    State coherence_state = CoherencePolicy::default_state();
};

// -------------------- Cache Set -----------------------
template <typename LineType, template <typename> class EvictionPolicy>
struct Set {
    vector<LineType> ways;
    EvictionPolicy<LineType> eviction;

    Set(size_t assoc) : ways(assoc), eviction() {}

    int choose_victim() { return eviction.choose_victim(ways); }
    void touch(int line_idx) { eviction.touch(line_idx); }
};

// -------------------------------------------------------
// |--------------------- Cache -------------------------|
// -------------------------------------------------------
template <typename CoherencePolicy, template <typename> class EvictionPolicy>
class Cache : public ICache {
    using LineType = Line<CoherencePolicy>;
    using SetType  = Set<LineType, EvictionPolicy>;

    EventSimulator& sim;  // cache pushes internal events to event_q
    SnoopBus& bus;
    Logger& logger;
    CoherencePolicy coherence;
    vector<SetType> sets;
                          //
    // cache size parameters 
    size_t mm_size;
    size_t blk_size;
    size_t num_sets;
    size_t assoc; 

    // addr bits 
    int blk_offset;
    int set_bits;
    int tag_bits;

    // latency per action
    int rd_hit_lt;
    int rd_miss_lt;
    int wr_hit_lt;
    int wr_miss_lt;
    int cache_id;
    string cache_name;

public:
    Cache(string name, size_t blk_size, size_t num_sets, size_t assoc, size_t mm_size, int rd_hit_lt, int rd_miss_lt, int wr_hit_lt, int wr_miss_lt, EventSimulator& sim, SnoopBus& bus, Logger& logger);

    // Main cache functions 
    LineType* find_line(uint64_t set_idx, uint64_t tag);
    void read(uint64_t addr) override;
    void write(uint64_t addr) override;
    void snoop_read(uint64_t addr) override;
    void snoop_write(uint64_t addr) override;
    std::string name() const override;

    // Helper functions 
    static size_t log2(size_t n);
    int index_in_set(SetType& set, LineType& line);
};

template<typename CoherencePolicy, template <typename> class EvictionPolicy>
Cache<CoherencePolicy, EvictionPolicy>::Cache(string name, size_t blk_size, size_t num_sets, size_t assoc, size_t mm_size, int rd_hit_lt, int rd_miss_lt, int wr_hit_lt, int wr_miss_lt, EventSimulator& sim, SnoopBus& bus, Logger& logger) 
    : cache_name(std::move(name)), blk_size(blk_size), num_sets(num_sets), assoc(assoc), mm_size(mm_size), rd_hit_lt(rd_hit_lt), rd_miss_lt(rd_miss_lt), wr_hit_lt(wr_hit_lt), wr_miss_lt(wr_miss_lt), sets(num_sets, SetType(assoc)), sim(sim), bus(bus), logger(logger) {
        blk_offset = log2(blk_size);
        set_bits   = log2(num_sets);
        tag_bits   = log2(mm_size) - (blk_offset + set_bits);
        bus.register_cache(this);
    }

template <typename CoherencePolicy, template <typename> class EvictionPolicy>
std::string Cache<CoherencePolicy, EvictionPolicy>::name() const {
    return cache_name;
}

// Cache will have following functions:
//  -- 1. find_line()
//  -- 2. read()
//  -- 3. write() 
//  -- 4. snoop_read()
//  -- 5. snoop_write()

// -------------------------------------------------------
// 1. cache.find_line()                                  |
// -------------------------------------------------------
//      -- Used to see if a cache block is present or not 
//      -- Returns line if found, else nullptr 
template<typename CoherencePolicy, template <typename> class EvictionPolicy>
typename Cache<CoherencePolicy, EvictionPolicy>::LineType* Cache<CoherencePolicy, EvictionPolicy>::find_line(uint64_t set_idx, uint64_t tag){
    for (auto &line : sets[set_idx].ways){
        if(line.valid && line.tag == tag)
            return &line;
    }
    return nullptr;
}

// -------------------------------------------------------
// 2, cache.read()                                       | 
// -------------------------------------------------------
//      -- cache.read(addr) is put into Event as 'action' from top 
//      -- time send along with read into the event is time at which read req is made 
//      -- Once 'read' is processed in event_q, schedule 'Hit' or 'Miss' 
template<typename CoherencePolicy, template <typename> class EvictionPolicy>
void Cache<CoherencePolicy, EvictionPolicy>::read(uint64_t addr){
    uint64_t set_idx = (addr >> blk_offset) & ((1 << set_bits) - 1);
    uint64_t tag     = (addr >> (blk_offset + set_bits)) & ((1 << tag_bits) - 1); 

    auto& set  = sets[set_idx];
    auto* line = find_line(set_idx, tag);
    
    // ----------------- READ HIT --------------- 
    if (line && coherence.can_read(line->coherence_state)){
        logger.log(sim.now(), "Cache_" + cache_name + " :: READ_HIT for addr(" + to_string(addr) + ")");
        sim.schedule(rd_hit_lt, [this, &set, line, addr]() mutable {
            set.touch(index_in_set(set, *line));
            logger.log(sim.now(), "Cache_" + cache_name + " :: LINE RETURNED for addr(" + to_string(addr) + ")");
            });
    }
    // ----------------- READ MISS -------------- 
    else {
        logger.log(sim.now(), "Cache_" + cache_name + " :: READ_MISS for addr(" + to_string(addr) + ")");
        uint64_t snoop_lt = bus.broadcast_snoop(this, false, addr);
        sim.schedule(snoop_lt + rd_miss_lt, [this, &set, tag, addr]() mutable {
            int victim_idx = set.choose_victim();
            auto* line = &set.ways[victim_idx];
            line->valid = true;
            line->tag   = tag; 
            coherence.on_read_miss(line->coherence_state);
            set.touch(victim_idx);
            logger.log(sim.now(), "Cache_" + cache_name + " :: LINE RETURNED for addr(" + to_string(addr) + ")");
            });
    }

}

// -------------------------------------------------------
// 3, cache.write()                                      | 
// -------------------------------------------------------
template<typename CoherencePolicy, template <typename> class EvictionPolicy>
void Cache<CoherencePolicy, EvictionPolicy>::write(uint64_t addr){
    uint64_t set_idx = (addr >> blk_offset) & ((1 << set_bits) - 1);
    uint64_t tag     = (addr >> (blk_offset + set_bits)) & ((1 << tag_bits) - 1); 

    auto& set  = sets[set_idx];
    auto* line = find_line(set_idx, tag);

    // ----------------- WRITE HIT --------------- 
    if (line){
        logger.log(sim.now(), "Cache_" + cache_name + " :: WRITE_HIT for addr(" + to_string(addr) + ")");
        if (coherence.can_write(line->coherence_state)){ // Line is in I, M or E state
            sim.schedule(wr_hit_lt, [this, &set, line, addr]() mutable {
                set.touch(index_in_set(set, *line));
                coherence.on_write(line->coherence_state); // changes to M
                logger.log(sim.now(), "Cache_" + cache_name + " :: LINE WRITTEN for addr(" + to_string(addr) + ")");
                });
        } 
        else{  // Line is in S state
            uint64_t snoop_lt = bus.broadcast_snoop(this, true, addr); // broadcast Invalidate to other sharers
            sim.schedule(snoop_lt + wr_hit_lt, [this, &set, line, addr]() mutable {
                set.touch(index_in_set(set, *line));
                coherence.on_write(line->coherence_state); // changes to M
                logger.log(sim.now(), "Cache_" + cache_name + " :: LINE WRITTEN for addr(" + to_string(addr) + ")");
                });
        }
    }
    // ----------------- WRITE MISS --------------- 
    else {
        logger.log(sim.now(), "Cache_" + cache_name + " :: WRITE_MISS for addr(" + to_string(addr) + ")");
        uint64_t snoop_lt = bus.broadcast_snoop(this, true, addr); // broadcast Invalidate to other sharers
        sim.schedule(snoop_lt + wr_miss_lt, [this, &set, tag, addr]() mutable {
            int victim_idx = set.choose_victim();
            auto* line = &set.ways[victim_idx];
            line->valid = true;
            line->tag = tag; 
            coherence.on_write(line->coherence_state); // changes to M
            logger.log(sim.now(), "Cache_" + cache_name + " :: LINE WRITTEN for addr(" + to_string(addr) + ")");
            });
    }
}

// -------------------------------------------------------
// 4. cache.snoop_read()                                 | 
// -------------------------------------------------------
template<typename CoherencePolicy, template <typename> class EvictionPolicy>
void Cache<CoherencePolicy, EvictionPolicy>::snoop_read(uint64_t addr){
    uint64_t set_idx = (addr >> blk_offset) & ((1 << set_bits) - 1);
    uint64_t tag     = (addr >> (blk_offset + set_bits)) & ((1 << tag_bits) - 1); 
    //auto& set        = sets[set_idx];
    auto* line       = find_line(set_idx, tag);
    if(line){
        coherence.on_snoop_read(line->coherence_state);
    }
    
}

// -------------------------------------------------------
// 5. cache.snoop_write()                                | 
// -------------------------------------------------------
template <typename CoherencePolicy, template <typename> class EvictionPolicy>
void Cache<CoherencePolicy, EvictionPolicy>::snoop_write(uint64_t addr){
    uint64_t set_idx = (addr >> blk_offset) & ((1 << set_bits) - 1);
    uint64_t tag     = (addr >> (blk_offset + set_bits)) & ((1 << tag_bits) - 1); 
    //auto& set        = sets[set_idx];
    auto* line       = find_line(set_idx, tag);
    if(line){
        coherence.on_snoop_write(line->coherence_state);
    }
}

// helper
template<typename CoherencePolicy, template <typename> class EvictionPolicy>
size_t Cache<CoherencePolicy, EvictionPolicy>::log2(size_t n){
    size_t res = 0;
    while(n >>= 1) res++;
    return res;
}
template<typename CoherencePolicy, template <typename> class EvictionPolicy>
int Cache<CoherencePolicy, EvictionPolicy>::index_in_set(SetType& set, LineType& line){
    return &line - &set.ways[0];
}

