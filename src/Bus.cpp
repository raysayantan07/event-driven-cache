#include "Bus.hpp"
#include "Cache.hpp"
#include <ios>
#include <sstream>

Bus::Bus(EventSimulator& sim, Logger& logger) 
    : sim(sim), logger(logger) {}

void Bus::register_cache(ICache* cache) {
    caches.push_back(cache);
}

void Bus::request_grant(const BusReq& req) {
    queue.push_back(req);
    if (!bus_busy) {
        bus_busy = true;
        // schedule the next process 
        sim.schedule(sim.now(), [this](){ this->process_next(); });
    }
}

// Call this function whenever some bus grant request ends 
void Bus::process_next() {
    if (queue.empty()){
        bus_busy = false;
        return;
    }

    BusReq req = queue.front();
    queue.pop_front();

    std::ostringstream oss;
    oss << "Bus :: processing (type = " << static_cast<int>(req.type)
        << ") from Cache_" << req.source->name() 
        << " addr(0x" << std::hex << req.addr << std::dec << ")";
    logger.log(sim.now(), oss.str());

    //Execute based on request type 
    switch(req.type){
        case BusReqType::SNOOP_READ:
            execute_snoop(req);
            break;
        case BusReqType::SNOOP_WRITE:
            execute_snoop(req);
            break;
        case BusReqType::READ_MISS_SERVICE:
            execute_data_service(req);
            break;
        case BusReqType::WRITE_MISS_SERVICE:
            execute_data_service(req);
            break;
        case BusReqType::INVALIDATE:
            execute_invalidate(req);
            break;
    }
}

void Bus::execute_snoop(const BusReq& req){
    // count how many target caches for snoop 
    int targets = 0;
    for (auto* cache : caches){
        if (cache != req.source) ++targets;
    }
    
    // shared state for aggregating snoop responses 
    struct SnoopState {
        BusReq req;
        int remaining = 0;
        bool any_success = false; 
        Bus* bus = nullptr;
    };
    auto state = std::make_shared<SnoopState>();
    state->req = req;
    state->bus = this;
    state->remaining = targets; 

    // if no other caches, complete immediately after delay 
    if (targets == 0){
        sim.schedule(sim.now() + req.delay, [state](){
            if (state->req.callback){
                state->req.callback(false);  // no data found 
            }
            // continue with next bus request 
            state->bus->sim.schedule(state->bus->sim.now(), [state](){ state->bus->process_next(); });
        });
        return;
    }

    // else, schedule snoop to each target cache 
    for (auto* cache : caches) {
        if (cache == req.source) continue;

        sim.schedule(sim.now() + req.delay, [cache, state](){
            bool snoop_success = false; 
            if (state->req.type == BusReqType::SNOOP_WRITE || state->req.type == BusReqType::INVALIDATE) {
                snoop_success = cache->snoop_write(state->req.addr);
            } else {
                snoop_success = cache->snoop_read(state->req.addr);
            }
            if (snoop_success)  state->any_success = true; 

            // log snoop response 
            {
                std::ostringstream oss;
                oss << "Bus :: Cache_" << state->req.source->name()
                    << " snooped Cache_" << cache->name()
                    << " addr(0x" << std::hex << state->req.addr << std::dec            
                    << ") --> " << (snoop_success ? "SNOOP_HIT" : "SNOOP_MISS");
                state->bus->logger.log(state->bus->sim.now(), oss.str());
            }

            // last responder triggers completion of SNOOP broadcast 
            if (--(state->remaining) == 0) {
                state->bus->sim.schedule(state->bus->sim.now(), [state](){
                    if (state->req.callback) {
                        state->req.callback(state->any_success);
                    }
                    // continue with next bus request 
                    state->bus->sim.schedule(state->bus->sim.now(), [state](){ state->bus->process_next(); });
                });
            }
        });
    }
}

// TODO : for now this simulates main memory. Connect top/main_mem module later 
void Bus::execute_data_service(const BusReq& req) {
    // Simulates Main memory serving the data 
    sim.schedule(sim.now() + req.delay, [this, req](){
        std::ostringstream oss;
        oss << "Bus :: Data service completed for Cache_" << req.source->name()
            << " addr(0x" << std::hex << req.addr << std::dec << ")";
        logger.log(sim.now(), oss.str());

        if(req.callback) req.callback(true); // true because we assume data always available at higher level 
     
        // Continue with next bus request 
        sim.schedule(sim.now(), [this](){ this->process_next(); });
    });
}

void Bus::execute_invalidate(const BusReq& req){
    // broadcast invalidate to all caches except source 
    int targets = 0;
    for (auto* cache : caches){
        if (cache != req.source) ++targets;
    }
    
    struct InavlidateState {
        BusReq req;
        int remaining = 0;
        Bus* bus = nullptr;
    };
    auto state = std::make_shared<InavlidateState>();
    state->bus = this;
    state->remaining = targets; 
    state->req = req; 

    // if there are no other caches, complete the request immediately
    if (targets == 0) {
        sim.schedule(sim.now() + req.delay, [state]() {
            if (state->req.callback) state->req.callback(false);
            state->bus->sim.schedule(state->bus->sim.now(), [state]() { state->bus->process_next(); });
        });
        return;
    }

    for ( auto* cache : caches ) {
        if (cache == req.source) continue;

        sim.schedule(sim.now() + req.delay, [cache, state]() {
            cache->snoop_write(state->req.addr); // invalidate is a form of snoop_write 
            
            std::ostringstream oss;
            oss << "Bus :: Cache_" << state->req.source->name() 
                << " invalidated Cache_" << cache->name() 
                << " addr(0x" << std::hex << state->req.addr << std::dec <<")";
            state->bus->logger.log(state->bus->sim.now(), oss.str());

            if (--(state->remaining) == 0) {
                state->bus->sim.schedule(state->bus->sim.now(), [state](){
                    if (state->req.callback) state->req.callback(true);
                    state->bus->sim.schedule(state->bus->sim.now(), [state](){ state->bus->process_next(); });
                });
            }
        });
    }

}















