#pragma once
#include <cstdint>
#include <string>
#include <iostream>
#include <iomanip>

#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"
#define GREY    "\033[38;5;245m"

struct Logger {
    virtual void log(uint64_t time, const std::string& msg) = 0;
    virtual ~Logger() = default;
};

struct ConsoleLogger : Logger {
    // Fixed-width time column (left-justified) for aligned output
    void log(uint64_t time, const std::string& msg) override {

        // Split message around cache name --> "Cache_L1A :: "
        std::string prefix = msg;
        std::string rest;
        const std::string key = "Cache_L1A :: ";
        size_t pos = msg.find(key);
        if(pos != std::string::npos){
            prefix = msg.substr(0, pos + key.size());
            rest   = msg.substr(pos + key.size());
        }


        std::string color = RESET; // default 
        if (rest.find("MISS") != std::string::npos)
            color = RED;
        else if (rest.find("HIT") != std::string::npos)
            color = GREEN;
        else if (rest.find("LINE RETURNED") != std::string::npos)
            color = YELLOW;
        else if (rest.find("LINE WRITTEN") != std::string::npos)
            color = YELLOW;
        else if (rest.find("READ_REQUEST") != std::string::npos)
            color = CYAN;
        else if (rest.find("WRITE_REQUEST") != std::string::npos)
            color = MAGENTA;
        constexpr int TIME_COL_WIDTH = 7;
        std::cout << "@ " << std::left << std::setw(TIME_COL_WIDTH) << time 
                  << " "  << std::right << prefix << color << rest << RESET << std::endl;
    }
};
