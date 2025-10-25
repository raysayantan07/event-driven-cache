#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <iostream>
#include "EventSimulator.hpp"
#include "Coherence.hpp"
#include "Eviction.hpp"
#include "SnoopBus.hpp"
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
    Cache(string name, size_t blk_size, size_t num_sets, size_t assoc, size_t mm_size, int rd_hit_lt, int rd_miss_lt, int wr_hit_lt, int wr_miss_lt, EventSimulator& sim, SnoopBus& bus);

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


