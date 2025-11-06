# Project Roadmap — event-driven-cache

Status: WIP  
Last updated: 2025-11-05

## Vision
A small, configurable event-driven cache simulator with pluggable coherence and eviction policies for experiments and teaching.

## How to use this file
- High-level goals and milestones live here.
- Track actionable work in GitHub Issues / Milestones and link them.
- Update this file (and the "Last updated" date) as items are completed.

## ALPHA build objectives
1. Basic cache implementation with one eviction and one coherence policy
   - [x] basic cache + one eviction + one coherence policy
2. Instantiate a single cache and verify the following:
   - [x] 2.1 verify read misses and hits
   - [x] 2.2 miss coalescing for in-flight misses 
   - [x] 2.3 verify LRU eviction implementation
   - [ ] 2.4 verify writes
   - [ ] 2.5 verify reads and writes
3. Create a proper Test bench system:
   - [ ] 3.1 Cache driver (CPU)
   - [ ] 3.2 Cache driver (MM)
   - [ ] 3.3 Create Unit Tests 

Notes:
- Goal: produce a tested single-cache simulator. Implement miss coalescing to avoid duplicate snoops for in-flight misses.

## BETA build objectives
1. Instantiate two coherent caches and complete the following:
   - [ ] 1.1 Implement correct snooping timing mechanics based on successful/failed snoops
   - [ ] 1.2 verify snooping on reads (unit tests)
   - [ ] 1.3 verify snooping on writes (unit tests)
   - [ ] 1.4 verify snooping on reads + writes (unit tests)

Notes:
- Goal: ensure correctness of snoop interactions and timing between caches; add cross-cache unit tests.

## v1.0 build objectives
- [ ] 1. Multi-level cache
- [ ] 2. Latest coherence policies
- [ ] 3. Latest eviction policies

## Current sprint / focus
- Implement miss coalescing and add unit tests for: miss coalescing, writes, read+write, eviction.
- Instrumentation: log snoop latencies and event scheduling times to validate timing behavior.

## Backlog / Nice-to-have
- GUI visualizer for event timeline
- YAML/JSON config for multi-cache setups
- CSV/trace export and visualization hooks
- Benchmarks and example workloads
- CI with unit tests and static analysis

## Contributing
- Open an Issue for any task; reference the issue when checking off items.
- Use clear commit messages and link PRs to issues.
- Update "Last updated" date when modifying this file.
