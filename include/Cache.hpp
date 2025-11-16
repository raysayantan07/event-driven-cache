#pragma once
#include <codecvt>
#include <cstdint>
#include <vector>
#include <string>
#include <iostream>
#include "EventSimulator.hpp"
#include "Coherence.hpp"
#include "Eviction.hpp"
#include "Bus.hpp"
#include "Logger.hpp"
using namespace std;

// -------------------- Base cache ----------------------
class ICache {
public:
    virtual bool snoop_read(uint64_t addr) = 0;
    virtual bool snoop_write(uint64_t addr) = 0;
    virtual void rd_miss_callback(bool snoop_success, uint64_t addr) = 0;
    virtual void wr_miss_callback(bool snoop_success, uint64_t addr) = 0;
    virtual void read(uint64_t addr) = 0;
    virtual void write(uint64_t addr) = 0;
    virtual std::string name() const = 0;
    virtual ~ICache() = default;
};

// --------------------- MSHR ENTRY ---------------------
struct MSHREntry {
    uint64_t blk_tag = 0;
    bool     valid   = false;
    // TDOD: waiters queue? 
};

struct MSHR {
    vector<MSHREntry> table;
    int mshr_count;
    MSHR(int mshr_count) : mshr_count(mshr_count) {
        table.resize(mshr_count);
        for ( auto& entry : table) {
            entry.blk_tag = 0;
            entry.valid   = false;
        }
    }
    void allocate_mshr(uint64_t blk_tag){
        for (auto& entry : table) {
            if (entry.valid == false) {
                entry.valid = true;
                entry.blk_tag = blk_tag;
                break;
            }
        }
    }
    void deallocate_mshr(uint64_t blk_tag){
        for (auto& entry : table) {
            if (entry.blk_tag == blk_tag) {
                entry.blk_tag = 0;
                entry.valid = false;
                break;
            }
        }
    }
    bool is_mshr_present(uint64_t blk_tag){
        for (auto& entry : table){
            if (blk_tag == entry.blk_tag && entry.valid) return true;
        }
        return false;
    }
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

    CoherencePolicy coherence;

    string cache_name;
    MSHR mshr;
    // cache size parameters 
    size_t blk_size;
    size_t num_sets;
    size_t assoc; 
    size_t mm_size;

    // latency per action
    int rd_hit_lt;
    int rd_miss_lt;
    int wr_hit_lt;
    int wr_miss_lt;
    int snoop_lt;
    int snoop_hit_lt;

    vector<SetType> sets;

    EventSimulator& sim;  // cache pushes internal events to event_q
    Bus& bus;
    Logger& logger;


    // addr bits 
    int blk_offset;
    int set_bits;
    int tag_bits;


public:
    Cache(string name, size_t blk_size, size_t num_sets, size_t assoc, size_t mm_size, int rd_hit_lt, int rd_miss_lt, int wr_hit_lt, int wr_miss_lt, int snoop_lt, int snoop_hit_lt, EventSimulator& sim, Bus& bus, Logger& logger);

    // Main cache functions 
    LineType* find_line(uint64_t set_idx, uint64_t tag);
    void read(uint64_t addr) override;
    void write(uint64_t addr) override;
    bool snoop_read(uint64_t addr) override;
    bool snoop_write(uint64_t addr) override;
    void rd_miss_callback(bool snoop_success, uint64_t addr) override;
    void wr_miss_callback(bool snoop_success, uint64_t addr) override;
    std::string name() const override;

    // Helper functions 
    static size_t log2(size_t n);
    int index_in_set(SetType& set, LineType& line);
};

template<typename CoherencePolicy, template <typename> class EvictionPolicy>
Cache<CoherencePolicy, EvictionPolicy>::Cache(string name, size_t blk_size, size_t num_sets, size_t assoc, size_t mm_size, int rd_hit_lt, int rd_miss_lt, int wr_hit_lt, int wr_miss_lt, int snoop_lt, int snoop_hit_lt, EventSimulator& sim, Bus& bus, Logger& logger) 
    : cache_name(std::move(name)), mshr(16), blk_size(blk_size), num_sets(num_sets), assoc(assoc), mm_size(mm_size), rd_hit_lt(rd_hit_lt), rd_miss_lt(rd_miss_lt), wr_hit_lt(wr_hit_lt), wr_miss_lt(wr_miss_lt), snoop_lt(snoop_lt), snoop_hit_lt(snoop_hit_lt), sets(num_sets, SetType(assoc)), sim(sim), bus(bus), logger(logger) {
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
    logger.log(sim.now(), "Cache_" + cache_name + " :: READ_REQUEST for addr(" + to_string(addr) + 
                          ") --> on SET[" + to_string(set_idx) + "] with TAG[" + to_string(tag) + "]");
    
    // ----------------- READ HIT --------------- 
    if (line && coherence.can_read(line->coherence_state)){
        logger.log(sim.now(), "Cache_" + cache_name + " ::  --> READ_HIT for addr(" + to_string(addr) + ")");
        sim.schedule(sim.now() + rd_hit_lt, [this, &set, line, addr]() mutable {
            set.touch(index_in_set(set, *line));
            logger.log(sim.now(), "Cache_" + cache_name + " :: LINE RETURNED for addr(" + to_string(addr) + ")");
            });
    }
    // ----------------- READ MISS -------------- 
    else {
        logger.log(sim.now(), "Cache_" + cache_name + " ::  --> READ_MISS for addr(" + to_string(addr) + ")");
        // if MSHR entry already present, merge miss, no need to schedule another miss 
        if (mshr.is_mshr_present(tag)) {
            logger.log(sim.now(), "Cache_" + cache_name + " ::  --> READ_MISS for addr(" + to_string(addr) + ") exists in MSHR --> COALESCED");
            return;
        }

        // if MSHR entry not present, create new miss and new MSHR entry 
        mshr.allocate_mshr(tag);
        //bus.broadcast_snoop(this, false, true, addr, snoop_lt);

        // request bus_grant for a snoop broadcast 
        BusReq req(BusReqType::SNOOP_READ, this, addr, snoop_lt); 
        req.callback = [this, addr](bool snoop_success){
            this->rd_miss_callback(snoop_success, addr);
        };
        bus.request_grant(req);
    }

}

template<typename CoherencePolicy, template <typename> class EvictionPolicy>
void Cache<CoherencePolicy, EvictionPolicy>::rd_miss_callback(bool snoop_success, uint64_t addr){
    uint64_t set_idx = (addr >> blk_offset) & ((1 << set_bits) - 1);
    uint64_t tag     = (addr >> (blk_offset + set_bits)) & ((1 << tag_bits) - 1); 

    //auto* line = find_line(set_idx, tag);
    int miss_latency = ((snoop_success == true) ? snoop_hit_lt : rd_miss_lt);

    BusReq req(BusReqType::READ_MISS_SERVICE, this, addr, miss_latency);
    req.callback = [this, set_idx, tag, addr](int success){   // success is always true 
        auto& set = sets[set_idx];
        int victim_idx = set.choose_victim();
        auto* line = &set.ways[victim_idx];
        line->valid = true;
        line->tag   = tag; 
        coherence.on_read_miss(line->coherence_state);
        set.touch(victim_idx);
        logger.log(sim.now(), "Cache_" + cache_name + " :: LINE RETURNED for addr(" + to_string(addr) + ")");
        mshr.deallocate_mshr(tag);
    };
    bus.request_grant(req);
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
    logger.log(sim.now(), "Cache_" + cache_name + " :: WRITE_REQUEST for addr(" + to_string(addr) + 
                          ") --> on SET[" + to_string(set_idx) + "] with TAG[" + to_string(tag) + "]");

    // ----------------- WRITE HIT --------------- 
    if (line){
        logger.log(sim.now(), "Cache_" + cache_name + " ::  --> WRITE_HIT for addr(" + to_string(addr) + ")");
        if (coherence.can_write(line->coherence_state)){ // Line is in I, M or E state
            sim.schedule(sim.now() + wr_hit_lt, [this, &set, line, addr]() mutable {
                set.touch(index_in_set(set, *line));
                coherence.on_write(line->coherence_state); // changes to M
                logger.log(sim.now(), "Cache_" + cache_name + " :: LINE WRITTEN for addr(" + to_string(addr) 
                           + ") -- (state:" + coherence.state_to_string(line->coherence_state) + " --> M)");
                });
        } 
        else{  // Line is in S state
            //bus.broadcast_snoop(this, true, false, addr, snoop_lt); // broadcast Invalidate to other sharers
            BusReq req(BusReqType::INVALIDATE, this, addr, snoop_lt);
            req.callback = [this, &set, line, addr](bool snoop_success) {
                sim.schedule(sim.now() + wr_hit_lt, [this, &set, line, addr]() mutable {
                    set.touch(index_in_set(set, *line));
                    coherence.on_write(line->coherence_state); // changes to M
                    logger.log(sim.now(), "Cache_" + cache_name + " :: LINE WRITTEN for addr(" + to_string(addr) + ") -- (state:S --> M)");
                });
            };
        }
    }
    // ----------------- WRITE MISS --------------- 
    else {
        logger.log(sim.now(), "Cache_" + cache_name + " ::  --> WRITE_MISS for addr(" + to_string(addr) + ")");
        // if MSHR entry already present, merge miss, no need to schedule another miss 
        if (mshr.is_mshr_present(tag)){ 
            logger.log(sim.now(), "Cache_" + cache_name + " ::  --> WRITE_MISS for addr(" + to_string(addr) + ") exists in MSHR --> COALESCED");
            return;
        }

        // if MSHR entry not present, create new miss and new MSHR entry 
        mshr.allocate_mshr(tag);
        //bus.broadcast_snoop(this, true, true, addr, snoop_lt); // broadcast Invalidate to other sharers
        BusReq req(BusReqType::SNOOP_WRITE, this, addr, snoop_lt);
        req.callback = [this, addr](bool snoop_success){
            this->wr_miss_callback(snoop_success, addr);
        };
        bus.request_grant(req);
    }
}
template<typename CoherencePolicy, template <typename> class EvictionPolicy>
void Cache<CoherencePolicy, EvictionPolicy>::wr_miss_callback(bool snoop_success, uint64_t addr){
    uint64_t set_idx = (addr >> blk_offset) & ((1 << set_bits) - 1);
    uint64_t tag     = (addr >> (blk_offset + set_bits)) & ((1 << tag_bits) - 1); 

    //auto* line = find_line(set_idx, tag);
    int miss_latency = ((snoop_success == true) ? snoop_hit_lt : wr_miss_lt);

    BusReq req(BusReqType::WRITE_MISS_SERVICE, this, addr, miss_latency);
    req.callback = [this, set_idx, tag, addr](bool success){
        auto& set = sets[set_idx];
        int victim_idx = set.choose_victim();
        auto* line = &set.ways[victim_idx];
        line->valid = true;
        line->tag = tag; 
        coherence.on_write(line->coherence_state); // changes to M
        logger.log(sim.now(), "Cache_" + cache_name + " :: LINE WRITTEN for addr(" + to_string(addr) + ") -- (state:I --> M)");
        mshr.deallocate_mshr(tag);
    };
    bus.request_grant(req);
}

// -------------------------------------------------------
// 4. cache.snoop_read()                                 | 
// -------------------------------------------------------
template<typename CoherencePolicy, template <typename> class EvictionPolicy>
bool Cache<CoherencePolicy, EvictionPolicy>::snoop_read(uint64_t addr){
    uint64_t set_idx = (addr >> blk_offset) & ((1 << set_bits) - 1);
    uint64_t tag     = (addr >> (blk_offset + set_bits)) & ((1 << tag_bits) - 1); 
    //auto& set        = sets[set_idx];
    auto* line       = find_line(set_idx, tag);
    if(line){
        coherence.on_snoop_read(line->coherence_state);
        return true;
    }
    return false;
}

// -------------------------------------------------------
// 5. cache.snoop_write()                                | 
// -------------------------------------------------------
template <typename CoherencePolicy, template <typename> class EvictionPolicy>
bool Cache<CoherencePolicy, EvictionPolicy>::snoop_write(uint64_t addr){
    uint64_t set_idx = (addr >> blk_offset) & ((1 << set_bits) - 1);
    uint64_t tag     = (addr >> (blk_offset + set_bits)) & ((1 << tag_bits) - 1); 
    //auto& set        = sets[set_idx];
    auto* line       = find_line(set_idx, tag);
    if(line){
        coherence.on_snoop_write(line->coherence_state);
        return true;
    }
    return false;
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

