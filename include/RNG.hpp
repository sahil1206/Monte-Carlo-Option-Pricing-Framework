// Random Number Generator

#pragma once
#include <random>
#include <memory>

class RNG {
private:
    // Thread-local RNG support for parallelization
    thread_local static std::mt19937_64 generator;
    thread_local static bool isInitialized;
    std::normal_distribution<double> normalDist;

public:
    RNG();
    explicit RNG(unsigned long seed);
    static void setSeed(unsigned long seed);
    static void setRandomSeed();
    double getNormal();
};
