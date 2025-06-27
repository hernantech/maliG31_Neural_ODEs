#pragma once
#include <chrono>

class Timer {
public:
    void start() {
        start_time = std::chrono::high_resolution_clock::now();
    }
    
    double elapsed() const {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time);
        return duration.count() / 1000000.0; // Return seconds
    }
    
private:
    std::chrono::high_resolution_clock::time_point start_time;
}; 