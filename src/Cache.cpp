#pragma once
#include "Cache.hpp"
#include <cstdint>

template<typename CoherencePolicy, template <typename> class EvictionPolicy>
Cache<CoherencePolicy, EvictionPolicy>::Cache(string name, size_t blk_size, size_t num_sets, size_t assoc, size_t mm_size, int rd_hit_lt, int rd_miss_lt, int wr_hit_lt, int wr_miss_lt, EventSimulator& sim, SnoopBus& bus) 
    : cache_name(std::move(name)), blk_size(blk_size), num_sets(num_sets), assoc(assoc), 
      mm_size(mm_size), rd_hit_lt(rd_hit_lt), rd_miss_lt(rd_miss_lt),
      wr_hit_lt(wr_hit_lt), wr_miss_lt(wr_miss_lt), sets(num_sets, SetType(assoc)), sim(sim), bus(bus) {
        blk_offset = log2(blk_size);
        set_bits   = log2(num_sets);
        tag_bits   = log2(mm_size) - (blk_offset + set_bits);
        bus.register_cache(this);
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
        cout << "@ " << sim.now() << " Cache_" << cache_name 
             << " :: READ_HIT for addr(" << addr << ")" << endl;
        sim.schedule(rd_hit_lt, [=, &set](){
            set.touch(index_in_set(set, *line));
            cout << "@ " << sim.now() << "Cache_" << cache_name
                 << " :: LINE RETURNED for addr(" << addr << ")" << endl;
            });
    }
    // ----------------- READ MISS -------------- 
    else {
        cout << "@ " << sim.now() << " Cache_" << cache_name 
             << " :: READ_MISS for addr(" << addr << ")" << endl;
        uint64_t snoop_lt = bus.broadcast_snoop(this, false, addr);
        sim.schedule(snoop_lt + rd_miss_lt, [=]() {
            int victim_idx = set.choose_victim();
            line = &set.ways[victim_idx];
            line->valid = true;
            line->tag   = tag; 
            coherence.on_read_miss(line->coherence_state);
            set.touch(victim_idx);
            cout << "@ " << sim.now() << "Cache_" << cache_name
                 << " :: LINE RETURNED for addr(" << addr << ")" << endl;
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
        cout << "@ " << sim.now() << " Cache_" << cache_name 
             << " :: WRITE_HIT for addr(" << addr << ")" << endl;
        if (coherence.can_write(line->coherence_state)){ // Line is in I, M or E state
            sim.schedule(wr_hit_lt, [=](){
                set.touch(index_in_set(set, *line));
                coherence.on_write(line->coherence_state); // changes to M
                cout << "@ " << sim.now() << "Cache_" << cache_name
                     << " :: LINE WRITTEN for addr(" << addr << ")" << endl;
                });
        } 
        else{  // Line is in S state
            uint64_t snoop_lt = bus.broadcast_snoop(this, true, addr); // broadcast Invalidate to other sharers
            sim.schedule(snoop_lt + wr_hit_lt, [=](){
                set.touch(index_in_set(set, *line));
                coherence.on_write(line->coherence_state); // changes to M
                cout << "@ " << sim.now() << "Cache_" << cache_name
                     << " :: LINE WRITTEN for addr(" << addr << ")" << endl;
                });
        }
    }
    // ----------------- WRITE MISS --------------- 
    else {
        cout << "@ " << sim.now() << " Cache_" << cache_name 
             << " :: WRITE_MISS for addr(" << addr << ")" << endl;
        uint64_t snoop_lt = bus.broadcast_snoop(this, true, addr); // broadcast Invalidate to other sharers
        sim.schedule(snoop_lt + wr_miss_lt, [=](){
            int victim_idx = set.choose_victim();
            line = &set.ways[victim_idx];
            line->valid = true;
            line->tag = tag; 
            coherence.on_write(line->coherence_state); // changes to M
            cout << "@ " << sim.now() << "Cache_" << cache_name
                 << " :: LINE WRITTEN for addr(" << addr << ")" << endl;
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
    auto& set        = sets[set_idx];
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
    auto& set        = sets[set_idx];
    auto* line       = find_line(set_idx, tag);
    if(line){
        coherence.on_snoop_write(line->coherence_state);
    }
}

// helper
template<typename CoherencePolicy, template <typename> class EvictionPolicy>
static size_t Cache<CoherencePolicy, EvictionPolicy>::log2(size_t n){
    size_t res = 0;
    while(n >>= 1) res++;
    return res;
}
template<typename CoherencePolicy, template <typename> class EvictionPolicy>
int Cache<CoherencePolicy, EvictionPolicy>::index_in_set(SetType& set, LineType& line){
    return &line - &set.ways[0];
}
