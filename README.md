# event-driven-cache

Lightweight event-driven C++ cache simulator with pluggable coherence and eviction policies. Intended for experiments and learning — configurable caches, a simple event simulator, and unit tests. 

The simulator is event driven that maintains a priority queue based on an event's total latency. 
Multiple Caches can be instantiated and Coherence can be seen in action. 
This is a fast model with no actual data transfer. 

## Features
- Event-driven simulator (priority-queue scheduler)
- Cache core with read/write/snoop hooks
- Pluggable coherence and eviction policies (e.g. MESI, LRU)
- Simple Logger interface and ConsoleLogger
- Bus with queued grant requests.
- Roadmap and planned unit tests (see `ROADMAP.md`)

## Quickstart (dev container: Ubuntu 24.04)
Prerequisites:
- g++ (or clang) with C++17 support
- CMake (optional, recommended)
- make, git

Steps to compile and run:
1. make clean
2. make
3. ./bin/cache_sim    

If the project includes unit tests:
- After building with CMake, run:
  ctest --output-on-failure
- Or run the test binary produced in `build/` or `bin/`.

## Common tasks
- Run the simulator: edit `src/main.cpp` to change scenarios, rebuild and run.
- Add a new coherence/eviction policy: implement policy in `include/` and register in tests.
- Improve logging: implement a Logger subclass (e.g., file-based) and pass it into modules.

## Repository layout (typical)
- include/        — public headers (Cache.hpp, Logger.hpp, etc.)
- src/            — implementation and `main.cpp`
- tests/          — unit tests (gtest or similar)
- ROADMAP.md      — project roadmap and milestones
- README.md       — this file

## Roadmap & issues
See `ROADMAP.md` for ALPHA/BETA/v1.0 objectives. Use GitHub Issues & Milestones to track work and link items from the roadmap.

---  
Last updated: 2025-11-07
