#pragma once
#include <list>
#include <vector>

template <typename LineType>
struct IEvictionPolicy {
    virtual void touch(int line_idx) = 0;
    virtual int choose_victim(const std::vector<LineType>& ways) = 0;
    virtual ~IEvictionPolicy() = 0;
};

// -----------------------------------------------------
//    LRU EVICTION POLICY                              |
// -----------------------------------------------------
template <typename LineType>
struct LRUEviction : public IEvictionPolicy<LineType>{
    std::list<int> order;  // front = MRU, back = LRU
    
    void touch(int line_idx) override {
        order.remove(line_idx);
        order.push_front(line_idx);
    }

    int choose_victim(const std::vector<LineType>& ways) override {
        // Prefer invalid lines first 
        for (int i = 0; i < (int)ways.size(); i++)
            if (!ways[i].valid) return i;

        // otherwise, evict LRU
        if (order.empty()) {
            // Initialize order if not yet populated
            for (int i = 0; i < (int)ways.size(); i++)
                order.push_back(i);
        }
        int victim = order.back();
        order.pop_back();
        order.push_front(victim); // treat as accessed once evicted
        return victim;
    }
};
