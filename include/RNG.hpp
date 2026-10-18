// Random Number Generator

#pragma once
#include <random>
#include <memory>

class RNG {
private:

    // Phase 5 - Adds thread-local RNG support for parallelization.
    thread_local static std::mt19937_64 generator;
    std::normal_distribution<double> normalDist;

public:
    RNG();
    double getNormal();
};