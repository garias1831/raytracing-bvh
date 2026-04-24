#ifndef UTIL_TIMER_H
#define UTIL_TIMER_H

#include <chrono>
#include <iostream>
#include <string>

// Simple timer to measure execution of a function.
// Usage: create an instance of UtilTimer at the beginning of the function.
// when this object goes out of scope, the corresponding msg is printed to stdout
// along with the execution time (in ms) of the function.

class UtilTimer {
    public:
        UtilTimer(std::string msg) : msg(msg) {
            start = std::chrono::steady_clock::now();
        };

        ~UtilTimer() {
            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            std::cout << msg << ": " << duration.count() << " ms" << std::endl;
        }

    private:
        std::string msg;
        std::chrono::steady_clock::time_point start;
        // For timing recursive fns
};

#endif