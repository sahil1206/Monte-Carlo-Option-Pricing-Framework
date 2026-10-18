#include "Mediator.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>

int main() {
    try {
        // Generate timestamped CSV filename
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm;
#ifdef _WIN32
        localtime_s(&tm, &time_t); // Windows-safe version
#else
        localtime_r(&time_t, &tm); // POSIX version
#endif
        std::stringstream ss;
        ss << std::put_time(&tm, "results_%Y%m%d_%H%M.csv");
        std::string csvFilename = ss.str();

        auto start = std::chrono::high_resolution_clock::now(); // Start total timing
        SimulationMediator mediator(csvFilename);
        mediator.runSimulations();
        auto end = std::chrono::high_resolution_clock::now(); // End total timing
        auto totalElapsed = std::chrono::duration<double>(end - start);
        std::cout << "\nTotal Simulation Time (Multithreaded): " << totalElapsed.count() << " seconds\n";
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}