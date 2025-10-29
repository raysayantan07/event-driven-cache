#pragma once
#include <cstdint>
#include <string>
#include <iostream>
#include <iomanip>

struct Logger {
    virtual void log(uint64_t time, const std::string& msg) = 0;
    virtual ~Logger() = default;
};

struct ConsoleLogger : Logger {
    // Fixed-width time column (left-justified) for aligned output
    void log(uint64_t time, const std::string& msg) override {
        constexpr int TIME_COL_WIDTH = 7;
        std::cout << "@ " << std::left << std::setw(TIME_COL_WIDTH) << time << " " << std::right << msg << std::endl;
    }
};