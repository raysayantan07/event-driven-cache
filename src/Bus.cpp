#include "Bus.hpp"
#include "Cache.hpp"
#include <sstream>

Bus::Bus(EventSimulator& sim_, Logger& logger_)
    : sim(sim_), logger(logger_) {}

void Bus::register_cache(ICache* cache) {
    caches.push_back(cache);
}

void Bus::request_grant(const BusReq& req) {
    queue.push_back(req);
    if (!bus_busy) {
        bus_busy = true;
        // Schedule process_next to run at current sim time
        sim.schedule(sim.now(), [this](){ this->process_next(); });
    }
}

void Bus::process_next() {
    if (queue.empty()) {
        bus_busy = false;
        return;
    }

    BusReq req = queue.front();
    queue.pop_front();

    std::ostringstream oss;
    oss << "BUS :: Processing " << static_cast<int>(req.type)
        << " from Cache_" << req.source->name()
        << " addr(0x" << std::hex << req.addr << std::dec << ")";
    logger.log(sim.now(), oss.str());

    // Execute based on request type
    switch (req.type) {
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

void Bus::execute_snoop(const BusReq& req) {
    // Count target caches (all except source)
    int targets = 0;
    for (auto* c : caches) {
        if (c != req.source) ++targets;
    }

    // Shared state for aggregating snoop responses
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

    // If no other caches, complete immediately after delay
    if (targets == 0) {
        sim.schedule(sim.now() + req.delay, [state]() {
            if (state->req.callback) {
                state->req.callback(false);  // no data found
            }
            // Continue with next bus request
            state->bus->sim.schedule(state->bus->sim.now(), [bus = state->bus](){ bus->process_next(); });
        });
        return;
    }

    // Schedule snoop to each target cache
    for (auto* cache : caches) {
        if (cache == req.source) continue;

        sim.schedule(sim.now() + req.delay, [cache, state]() {
            bool snoop_success = false;
            if (state->req.type == BusReqType::SNOOP_WRITE || state->req.type == BusReqType::INVALIDATE) {
                snoop_success = cache->snoop_write(state->req.addr);
            } else {
                snoop_success = cache->snoop_read(state->req.addr);
            }

            if (snoop_success) {
                state->any_success = true;
            }

            // Log snoop response
            {
                std::ostringstream oss;
                oss << "BUS :: Cache_" << state->req.source->name()
                    << " snooped Cache_" << cache->name()
                    << " addr(0x" << std::hex << state->req.addr << std::dec << ") --> "
                    << (snoop_success ? "HIT" : "MISS");
                state->bus->logger.log(state->bus->sim.now(), oss.str());
            }

            // Last responder triggers completion
            if (--state->remaining == 0) {
                state->bus->sim.schedule(state->bus->sim.now(), [state]() {
                    if (state->req.callback) {
                        state->req.callback(state->any_success);
                    }
                    // Continue with next bus request
                    state->bus->sim.schedule(state->bus->sim.now(), [bus = state->bus](){ bus->process_next(); });
                });
            }
        });
    }
}

void Bus::execute_data_service(const BusReq& req) {
    // Simulates main memory or L2 serving the data
    sim.schedule(sim.now() + req.delay, [this, req]() {
        std::ostringstream oss;
        oss << "BUS :: Data service completed for Cache_" << req.source->name()
            << " addr(0x" << std::hex << req.addr << std::dec << ")";
        logger.log(sim.now(), oss.str());

        if (req.callback) {
            req.callback(true);  // data always successfully served
        }
        // Continue with next bus request
        this->sim.schedule(this->sim.now(), [this](){ this->process_next(); });
    });
}

void Bus::execute_invalidate(const BusReq& req) {
    // Broadcast invalidate to all caches except source
    int targets = 0;
    for (auto* c : caches) {
        if (c != req.source) ++targets;
    }

    struct InvalidateState {
        BusReq req;
        int remaining = 0;
        Bus* bus = nullptr;
    };
    auto state = std::make_shared<InvalidateState>();
    state->req = req;
    state->bus = this;
    state->remaining = targets;

    if (targets == 0) {
        sim.schedule(sim.now() + req.delay, [state]() {
            if (state->req.callback) {
                state->req.callback(true);
            }
            state->bus->sim.schedule(state->bus->sim.now(), [bus = state->bus](){ bus->process_next(); });
        });
        return;
    }

    for (auto* cache : caches) {
        if (cache == req.source) continue;

        sim.schedule(sim.now() + req.delay, [cache, state]() {
            cache->snoop_write(state->req.addr);  // treat invalidate as write snoop

            std::ostringstream oss;
            oss << "BUS :: Cache_" << state->req.source->name()
                << " invalidated Cache_" << cache->name()
                << " addr(0x" << std::hex << state->req.addr << std::dec << ")";
            state->bus->logger.log(state->bus->sim.now(), oss.str());

            if (--state->remaining == 0) {
                state->bus->sim.schedule(state->bus->sim.now(), [state]() {
                    if (state->req.callback) {
                        state->req.callback(true);
                    }
                    state->bus->sim.schedule(state->bus->sim.now(), [bus = state->bus](){ bus->process_next(); });
                });
            }
        });
    }
}