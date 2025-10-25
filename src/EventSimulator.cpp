#include "EventSimulator.hpp"

void EventSimulator::schedule(uint64_t time, std::function<void()> action){
    event_q.push(Event{now() + time, action});
}

void EventSimulator::run_sim(){
    while(!event_q.empty()){
        Event ev = event_q.top();
        event_q.pop();
        currentTime += ev.time;
        ev.action();
    }
}

uint64_t EventSimulator::now(){
    return currentTime;
}
