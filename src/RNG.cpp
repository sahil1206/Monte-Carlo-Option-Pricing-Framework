// Implements thread-local RNG with unique or fixed seeding.

#include "RNG.hpp"
#include <chrono>
#include <thread>
#include <atomic>

static std::atomic<bool> g_useCustomSeed{false};
static std::atomic<unsigned long> g_customSeed{0};

thread_local std::mt19937_64 RNG::generator(
    static_cast<unsigned long>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count() ^
        std::hash<std::thread::id>{}(std::this_thread::get_id())
    )
);

thread_local bool RNG::isInitialized = false;

RNG::RNG() : normalDist(0.0, 1.0) {}

RNG::RNG(unsigned long seed) : normalDist(0.0, 1.0) {
    setSeed(seed);
}

void RNG::setSeed(unsigned long seed) {
    g_useCustomSeed = true;
    g_customSeed = seed;
    generator.seed(seed ^ std::hash<std::thread::id>{}(std::this_thread::get_id()));
}

void RNG::setRandomSeed() {
    g_useCustomSeed = false;
    generator.seed(static_cast<unsigned long>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count() ^
        std::hash<std::thread::id>{}(std::this_thread::get_id())
    ));
}

double RNG::getNormal() {
    if (!isInitialized && g_useCustomSeed.load()) {
        generator.seed(g_customSeed.load() ^ std::hash<std::thread::id>{}(std::this_thread::get_id()));
        isInitialized = true;
    }
    thread_local static std::normal_distribution<double> dist(0.0, 1.0);
    return dist(generator);
}
