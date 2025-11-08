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
    
    static std::string cache_color(const std::string& cache){
        if (cache == "Cache_L1A") return BLUE;
        if (cache == "Cache_L1B") return RED;
        return RESET;
    }

    static std::string rest_color(const std::string& rest){
        if (rest.find("MISS") != std::string::npos)            return RED;
        if (rest.find("HIT") != std::string::npos)             return GREEN;
        if (rest.find("LINE RETURNED") != std::string::npos)   return YELLOW;
        if (rest.find("LINE WRITTEN") != std::string::npos)    return YELLOW;
        if (rest.find("READ_REQUEST") != std::string::npos)    return CYAN;
        if (rest.find("WRITE_REQUEST") != std::string::npos)   return MAGENTA;
        return RESET;
    }

    void log(uint64_t time, const std::string& msg) override {
        constexpr int TIME_COL_WIDTH = 7;

        // Split message around cache name --> "Cache_L1A :: "
        std::string prefix = msg;   // upto (including) " :: "
        std::string rest;           // after " :: "
        std::string cache_name;    // "Cache_L1*"

        // locate "Cache_" then the following " :: "
        size_t cpos = msg.find("Cache_");
        size_t dpos = msg.find(" :: ", cpos == std::string::npos ? 0 : cpos);
        if (cpos != std::string::npos && dpos != std::string::npos) {
            cache_name = msg.substr(cpos, dpos - cpos);  // e.g. - "Cache_L1A"
            prefix     = msg.substr(0, dpos + 4);        // + 4 includes " :: "
            rest       = msg.substr(dpos + 4);
        }

        // colorise cache label inside prefix 
        if (!cache_name.empty()){
            std::string colored_cache_name = cache_color(cache_name) + cache_name + RESET;
            prefix.replace(cpos, cache_name.size(), colored_cache_name);
        }

        // colorise 'rest' using keywords; split must consider L1A and L1B too 
        std::string color_rest = rest_color(rest);

        std::cout << "@ " << std::left << std::setw(TIME_COL_WIDTH) << time 
                  << " "  << std::right << prefix << color_rest << rest << RESET << std::endl;


    }
};
