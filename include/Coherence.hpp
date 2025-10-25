#pragma once
#include <cstdint>

struct MESICoherence {
    enum class StateType { I, S, E, M };
    static constexpr StateType default_state() { return StateType::I; }

    bool can_read(const StateType& state) const {
        return state != StateType::I;
    }
    bool can_write(const StateType& state) const {
        return state != StateType::S;
    }
    void on_read_hit(StateType& state) {
        // snoop to see if someone else has it 
        state = StateType::S; // or E 
    }
    void on_write(StateType& state) {
        state = StateType::M;
        // send Invalidate to all other caches 
    }
    void on_read_miss(StateType& state){
        if (state == StateType::I) 
            state = StateType::S;
    }

    // --------- Functions for read/write in other caches ------------
    // read req in different cache makes data in current cache 'Shared'
    void on_snoop_read(StateType& state) {
        if (state == StateType::M || state == StateType::E) state = StateType::S;
    }
    // write req in different cache invalidates data in current cache
    void on_snoop_write(StateType& state) {
        state = StateType::I;
    }
};
