#pragma once
#include <cstdint>
#include <vector>
#include <deque>
#include <functional>
#include <memory>
#include "EventSimulator.hpp"
#include "Logger.hpp"

class ICache; // forward declaration

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
    uint64_t delay = 0;  // latency for this request
    std::function<void(bool)> callback = nullptr;  // invoked when request completes

    BusReq() = default;
    BusReq(BusReqType t, ICache* src, uint64_t a, uint64_t d)
        : type(t), source(src), addr(a), delay(d) {}
};

class Bus {
public:
    Bus(EventSimulator& sim, Logger& logger);

    // Register a cache with the bus
    void register_cache(ICache* cache);

    // Request bus access. If bus is free, starts immediately; else queues request.
    // callback is invoked with success status when the request completes.
    void request_grant(const BusReq& req);

private:
    EventSimulator& sim;
    Logger& logger;
    std::vector<ICache*> caches;

    std::deque<BusReq> queue;
    bool bus_busy = false;

    // Process the head of the queue
    void process_next();

    // Execute a snoop broadcast (helper for SNOOP_READ / SNOOP_WRITE)
    void execute_snoop(const BusReq& req);

    // Execute a data service request (helper for READ_MISS_SERVICE / WRITE_MISS_SERVICE)
    void execute_data_service(const BusReq& req);

    // Execute an invalidate broadcast
    void execute_invalidate(const BusReq& req);
};