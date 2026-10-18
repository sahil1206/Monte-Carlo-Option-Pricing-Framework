// Implements thread-local RNG with unique seeding.

#include "RNG.hpp"
#include <chrono>
#include <thread>

thread_local std::mt19937_64 RNG::generator(
    static_cast<unsigned long>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count() ^
        std::hash<std::thread::id>{}(std::this_thread::get_id())
        )
);

RNG::RNG() : normalDist(0.0, 1.0) {}

double RNG::getNormal() {
    thread_local static std::normal_distribution<double> dist(0.0, 1.0);
    return dist(generator);
}