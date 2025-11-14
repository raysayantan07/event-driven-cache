#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <iostream>
#include <unordered_map>
#include "EventSimulator.hpp"
#include "Coherence.hpp"
#include "Eviction.hpp"
#include "Bus.hpp"
#include "Logger.hpp"
using namespace std;

// -------------------- MSHR Entry ----------------------
struct MSHREntry {
    uint64_t addr;
    uint64_t tag;
    uint64_t set_idx;
    int pending_requests = 0;  // count of coalesced requests
};

// -------------------- Base cache ----------------------
class ICache {
public:
    virtual bool snoop_read(uint64_t addr) = 0;
    virtual bool snoop_write(uint64_t addr) = 0;
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

    EventSimulator& sim;
    Bus& bus;
    Logger& logger;
    CoherencePolicy coherence;
    vector<SetType> sets;

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
    int snoop_lt;
    string cache_name;

    // MSHR: tracks in-flight misses by tag
    unordered_map<uint64_t, MSHREntry> mshr;
    int mshr_size = 16;  // configurable MSHR capacity

public:
    Cache(string name, size_t blk_size, size_t num_sets, size_t assoc, size_t mm_size, 
          int rd_hit_lt, int rd_miss_lt, int wr_hit_lt, int wr_miss_lt, int snoop_lt,
          EventSimulator& sim, Bus& bus, Logger& logger);

    // Main cache functions 
    LineType* find_line(uint64_t set_idx, uint64_t tag);
    void read(uint64_t addr) override;
    void write(uint64_t addr) override;
    bool snoop_read(uint64_t addr) override;
    bool snoop_write(uint64_t addr) override;
    std::string name() const override;

    // Helper functions 
    static size_t log2(size_t n);
    int index_in_set(SetType& set, LineType& line);

    // MSHR management
    bool has_mshr_entry(uint64_t tag);
    void add_mshr_entry(uint64_t addr, uint64_t tag, uint64_t set_idx);
    void remove_mshr_entry(uint64_t tag);
    void increment_mshr_pending(uint64_t tag);

    // Bus callbacks for miss resolution
    void on_read_miss_snoop_complete(bool snoop_success, uint64_t addr);
    void on_write_miss_snoop_complete(bool snoop_success, uint64_t addr);
};

// Template implementations
template<typename CoherencePolicy, template <typename> class EvictionPolicy>
Cache<CoherencePolicy, EvictionPolicy>::Cache(string name, size_t blk_size, size_t num_sets, size_t assoc, size_t mm_size, 
                                              int rd_hit_lt, int rd_miss_lt, int wr_hit_lt, int wr_miss_lt, int snoop_lt,
                                              EventSimulator& sim, Bus& bus, Logger& logger) 
    : cache_name(std::move(name)), blk_size(blk_size), num_sets(num_sets), assoc(assoc), mm_size(mm_size),
      rd_hit_lt(rd_hit_lt), rd_miss_lt(rd_miss_lt), wr_hit_lt(wr_hit_lt), wr_miss_lt(wr_miss_lt), snoop_lt(snoop_lt),
      sets(num_sets, SetType(assoc)), sim(sim), bus(bus), logger(logger) {
        blk_offset = log2(blk_size);
        set_bits   = log2(num_sets);
        tag_bits   = log2(mm_size) - (blk_offset + set_bits);
        bus.register_cache(this);
    }

template <typename CoherencePolicy, template <typename> class EvictionPolicy>
std::string Cache<CoherencePolicy, EvictionPolicy>::name() const {
    return cache_name;
}

template<typename CoherencePolicy, template <typename> class EvictionPolicy>
typename Cache<CoherencePolicy, EvictionPolicy>::LineType* Cache<CoherencePolicy, EvictionPolicy>::find_line(uint64_t set_idx, uint64_t tag){
    for (auto &line : sets[set_idx].ways){
        if(line.valid && line.tag == tag)
            return &line;
    }
    return nullptr;
}

// MSHR helpers
template<typename CoherencePolicy, template <typename> class EvictionPolicy>
bool Cache<CoherencePolicy, EvictionPolicy>::has_mshr_entry(uint64_t tag){
    return mshr.find(tag) != mshr.end();
}

template<typename CoherencePolicy, template <typename> class EvictionPolicy>
void Cache<CoherencePolicy, EvictionPolicy>::add_mshr_entry(uint64_t addr, uint64_t tag, uint64_t set_idx){
    mshr[tag] = MSHREntry{addr, tag, set_idx, 1};
    logger.log(sim.now(), "Cache_" + cache_name + " :: MSHR_ALLOC for addr(0x" + to_string(addr) + ")");
}

template<typename CoherencePolicy, template <typename> class EvictionPolicy>
void Cache<CoherencePolicy, EvictionPolicy>::remove_mshr_entry(uint64_t tag){
    mshr.erase(tag);
}

template<typename CoherencePolicy, template <typename> class EvictionPolicy>
void Cache<CoherencePolicy, EvictionPolicy>::increment_mshr_pending(uint64_t tag){
    if (mshr.find(tag) != mshr.end()) {
        mshr[tag].pending_requests++;
    }
}

template<typename CoherencePolicy, template <typename> class EvictionPolicy>
void Cache<CoherencePolicy, EvictionPolicy>::read(uint64_t addr){
    uint64_t set_idx = (addr >> blk_offset) & ((1 << set_bits) - 1);
    uint64_t tag     = (addr >> (blk_offset + set_bits)) & ((1 << tag_bits) - 1); 

    auto& set  = sets[set_idx];
    auto* line = find_line(set_idx, tag);
    
    // READ HIT
    if (line && coherence.can_read(line->coherence_state)){
        logger.log(sim.now(), "Cache_" + cache_name + " :: READ_HIT for addr(0x" + to_string(addr) + ")");
        sim.schedule(sim.now() + rd_hit_lt, [this, &set, line, addr]() mutable {
            set.touch(index_in_set(set, *line));
            logger.log(sim.now(), "Cache_" + cache_name + " :: LINE RETURNED for addr(0x" + to_string(addr) + ")");
        });
    }
    // READ MISS
    else {
        logger.log(sim.now(), "Cache_" + cache_name + " :: READ_MISS for addr(0x" + to_string(addr) + ")");
        
        // Check MSHR: if entry exists, coalesce; else allocate new MSHR entry and issue bus request
        if (has_mshr_entry(tag)) {
            increment_mshr_pending(tag);
            logger.log(sim.now(), "Cache_" + cache_name + " :: MSHR_COALESCE for addr(0x" + to_string(addr) + ")");
        } else {
            add_mshr_entry(addr, tag, set_idx);
            BusReq req(BusReqType::SNOOP_READ, this, addr, snoop_lt);
            req.callback = [this, &set, tag, addr](bool snoop_success) {
                this->on_read_miss_snoop_complete(snoop_success, addr);
            };
            bus.request_grant(req);
        }
    }
}

template<typename CoherencePolicy, template <typename> class EvictionPolicy>
void Cache<CoherencePolicy, EvictionPolicy>::write(uint64_t addr){
    uint64_t set_idx = (addr >> blk_offset) & ((1 << set_bits) - 1);
    uint64_t tag     = (addr >> (blk_offset + set_bits)) & ((1 << tag_bits) - 1); 

    auto& set  = sets[set_idx];
    auto* line = find_line(set_idx, tag);

    // WRITE HIT
    if (line){
        logger.log(sim.now(), "Cache_" + cache_name + " :: WRITE_HIT for addr(0x" + to_string(addr) + ")");
        if (coherence.can_write(line->coherence_state)){
            sim.schedule(sim.now() + wr_hit_lt, [this, &set, line, addr]() mutable {
                set.touch(index_in_set(set, *line));
                coherence.on_write(line->coherence_state);
                logger.log(sim.now(), "Cache_" + cache_name + " :: LINE WRITTEN for addr(0x" + to_string(addr) + ")");
            });
        } 
        else{  // Line is in S state
            BusReq req(BusReqType::INVALIDATE, this, addr, snoop_lt);
            req.callback = [this, &set, line, addr](bool success) {
                sim.schedule(sim.now() + wr_hit_lt, [this, &set, line, addr]() mutable {
                    set.touch(index_in_set(set, *line));
                    coherence.on_write(line->coherence_state);
                    logger.log(sim.now(), "Cache_" + cache_name + " :: LINE WRITTEN for addr(0x" + to_string(addr) + ")");
                });
            };
            bus.request_grant(req);
        }
    }
    // WRITE MISS
    else {
        logger.log(sim.now(), "Cache_" + cache_name + " :: WRITE_MISS for addr(0x" + to_string(addr) + ")");
        
        // Check MSHR: if entry exists, coalesce; else allocate new MSHR entry and issue bus request
        if (has_mshr_entry(tag)) {
            increment_mshr_pending(tag);
            logger.log(sim.now(), "Cache_" + cache_name + " :: MSHR_COALESCE for addr(0x" + to_string(addr) + ")");
        } else {
            add_mshr_entry(addr, tag, set_idx);
            BusReq req(BusReqType::SNOOP_WRITE, this, addr, snoop_lt);
            req.callback = [this, &set, tag, addr](bool snoop_success) {
                this->on_write_miss_snoop_complete(snoop_success, addr);
            };
            bus.request_grant(req);
        }
    }
}

template<typename CoherencePolicy, template <typename> class EvictionPolicy>
bool Cache<CoherencePolicy, EvictionPolicy>::snoop_read(uint64_t addr){
    uint64_t set_idx = (addr >> blk_offset) & ((1 << set_bits) - 1);
    uint64_t tag     = (addr >> (blk_offset + set_bits)) & ((1 << tag_bits) - 1); 
    auto* line       = find_line(set_idx, tag);
    if(line){
        coherence.on_snoop_read(line->coherence_state);
        return true;
    }
    return false;
}

template <typename CoherencePolicy, template <typename> class EvictionPolicy>
bool Cache<CoherencePolicy, EvictionPolicy>::snoop_write(uint64_t addr){
    uint64_t set_idx = (addr >> blk_offset) & ((1 << set_bits) - 1);
    uint64_t tag     = (addr >> (blk_offset + set_bits)) & ((1 << tag_bits) - 1); 
    auto* line       = find_line(set_idx, tag);
    if(line){
        coherence.on_snoop_write(line->coherence_state);
        return true;
    }
    return false;
}

template<typename CoherencePolicy, template <typename> class EvictionPolicy>
void Cache<CoherencePolicy, EvictionPolicy>::on_read_miss_snoop_complete(bool snoop_success, uint64_t addr){
    uint64_t set_idx = (addr >> blk_offset) & ((1 << set_bits) - 1);
    uint64_t tag     = (addr >> (blk_offset + set_bits)) & ((1 << tag_bits) - 1); 
    auto& set = sets[set_idx];

    sim.schedule(sim.now() + rd_miss_lt, [this, &set, tag, addr]() mutable {
        int victim_idx = set.choose_victim();
        auto* line = &set.ways[victim_idx];
        line->valid = true;
        line->tag = tag;
        coherence.on_read_miss(line->coherence_state);
        set.touch(victim_idx);
        logger.log(sim.now(), "Cache_" + cache_name + " :: LINE RETURNED for addr(0x" + to_string(addr) + ")");
        
        // Remove MSHR entry now that miss is complete
        remove_mshr_entry(tag);
    });
}

template<typename CoherencePolicy, template <typename> class EvictionPolicy>
void Cache<CoherencePolicy, EvictionPolicy>::on_write_miss_snoop_complete(bool snoop_success, uint64_t addr){
    uint64_t set_idx = (addr >> blk_offset) & ((1 << set_bits) - 1);
    uint64_t tag     = (addr >> (blk_offset + set_bits)) & ((1 << tag_bits) - 1); 
    auto& set = sets[set_idx];

    sim.schedule(sim.now() + wr_miss_lt, [this, &set, tag, addr]() mutable {
        int victim_idx = set.choose_victim();
        auto* line = &set.ways[victim_idx];
        line->valid = true;
        line->tag = tag;
        coherence.on_write(line->coherence_state);
        set.touch(victim_idx);
        logger.log(sim.now(), "Cache_" + cache_name + " :: LINE WRITTEN for addr(0x" + to_string(addr) + ")");
        
        // Remove MSHR entry now that miss is complete
        remove_mshr_entry(tag);
    });
}

template<typename CoherencePolicy, template <typename> class EvictionPolicy>
size_t Cache<CoherencePolicy, EvictionPolicy>::log2(size_t n){
    size_t res = 0;
    while(n >>= 1) res++;
    return res;
}

template<typename CoherencePolicy, template <typename> class EvictionPolicy>
int Cache<CoherencePolicy, EvictionPolicy>::index_in_set(SetType& set, LineType& line){
    return static_cast<int>(&line - &set.ways[0]);
}