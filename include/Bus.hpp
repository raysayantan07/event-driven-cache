#pragma once
#include <cstdint>
#include <vector>
#include <deque>
#include <functional>
#include "EventSimulator.hpp"
#include "Logger.hpp"

class ICache; // forward declaration
              //
enum class BusReqType {
    SNOOP_READ,
    SNOOP_WRITE,
    READ_MISS_SERVICE,
    WRITE_MISS_SERVICE,
    INVALIDATE
};

struct BusReq {
    BusReqType type;
    ICache* source = nullptr;
    uint64_t addr = 0;
    uint64_t delay = 0;     // latency for this request
    std::function<void(bool)> callback = nullptr; // invoked when request ends
    BusReq() = default; 
    BusReq(BusReqType t, ICache* src, uint64_t addr, uint64_t delay)
        : type(t), source(src), addr(addr), delay(delay) {}
};

class Bus {
public:
    Bus(EventSimulator& sim, Logger& logger);

    // Register a cache with the bus 
    void register_cache(ICache* cache);

    // Request for bus access 
    //  -- if bus is free, start immediately
    //  -- else queues the request 
    // callback is invoked with success status when the request completes 
    void request_grant(const BusReq& req);

private:
    EventSimulator& sim;
    Logger& logger;
    std::vector<ICache*> caches;

    std::deque<BusReq> queue;
    bool bus_busy = false;

    // Process next head of the queue 
    void process_next();

    // Execute a snoop broadcast (helper for SNOOP_READ / SNOOP_WRITE) 
    void execute_snoop(const BusReq& req);

    // Execute a data service request (helper for READ_MISS_SERVICE / WRITE_MISS_SERVICE)
    void execute_data_service(const BusReq& req);

    // Execute an Invalidate broadcast 
    void execute_invalidate(const BusReq& req);
};
