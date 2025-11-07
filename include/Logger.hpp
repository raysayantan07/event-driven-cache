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

struct Logger {
    virtual void log(uint64_t time, const std::string& msg) = 0;
    virtual ~Logger() = default;
};

struct ConsoleLogger : Logger {
    // Fixed-width time column (left-justified) for aligned output
    void log(uint64_t time, const std::string& msg) override {
        std::string color = RESET; // default 
        if (msg.find("MISS") != std::string::npos)
            color = RED;
        else if (msg.find("HIT") != std::string::npos)
            color = GREEN;
        else if (msg.find("LINE RETURNED") != std::string::npos)
            color = YELLOW;
        else if (msg.find("LINE WRITTEN") != std::string::npos)
            color = YELLOW;
        else if (msg.find("READ_REQUEST") != std::string::npos)
            color = CYAN;
        else if (msg.find("WRITE_REQUEST") != std::string::npos)
            color = MAGENTA;
        constexpr int TIME_COL_WIDTH = 7;
        std::cout << "@ " << std::left << std::setw(TIME_COL_WIDTH) << time << " " << color << std::right << msg << std::endl;
    }
};
