// main.cpp
// Entry point for Monte Carlo Option Pricing Engine
// Launches interactive TUI by default, or headless batch simulation with --batch
// Author: Sahil Jain

#include "TUI.hpp"
#include "Mediator.hpp"
#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>

int main(int argc, char* argv[]) {
    try {
        // If run with --batch or -b, execute the full non-interactive batch matrix
        if (argc > 1 && (std::string(argv[1]) == "--batch" || std::string(argv[1]) == "-b")) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::tm tm;
#ifdef _WIN32
            localtime_s(&tm, &time_t);
#else
            localtime_r(&time_t, &tm);
#endif
            std::stringstream ss;
            ss << std::put_time(&tm, "results_%Y%m%d_%H%M.csv");
            std::string csvFilename = ss.str();

            std::cout << "Starting Batch Grid Monte Carlo Simulation Matrix...\n";
            auto start = std::chrono::high_resolution_clock::now();
            SimulationMediator mediator(csvFilename);
            mediator.runSimulations();
            auto end = std::chrono::high_resolution_clock::now();
            auto totalElapsed = std::chrono::duration<double>(end - start);
            std::cout << "\nTotal Batch Simulation Time: " << totalElapsed.count() << " seconds\n";
            return 0;
        }

        // By default, launch the interactive TUI workbench
        TUI tui;
        tui.run();
    }
    catch (const std::exception& e) {
        std::cerr << "\n[Fatal Error]: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
