// MCEngine.hpp
// This file defines the MCEngine class, which implements the Monte Carlo simulation for option pricing.
// Author: Sahil Jain

#ifndef MCEngine_hpp
#define MCEngine_hpp

#pragma once
#include "SDE.hpp"
#include "RNG.hpp"
#include "Payoff.hpp"
#include <memory>
#include <concepts>
#include <chrono>

enum class FDScheme { Euler, Milstein, PredictorCorrector };

struct PricingResult {
    double price;
    double stdDev;
    double stdError;
    int hitsZero;
    std::chrono::duration<double> elapsedTime;
};

// C++20 Concepts
template<typename T>
concept SDEConcept = std::derived_from<T, SDEBase>&&
    requires(T t, double time, double S) {
        { t.drift(time, S) } -> std::same_as<double>;
        { t.diffusion(time, S) } -> std::same_as<double>;
        { t.diffusionDerivative(time, S) } -> std::same_as<double>;
        { t.initialCondition() } -> std::same_as<double>;
};

template<typename T>
concept PayoffConcept = requires(T t, double S) {
    { t(S) } -> std::same_as<double>;
    { t.getExpiry() } -> std::same_as<double>;
    { t.getRate() } -> std::same_as<double>;
    { t.checkBarrier(S) } -> std::same_as<void>;
    { t.accumulatePrice(S) } -> std::same_as<void>;
    { t.resetBarrier() } -> std::same_as<void>;
    { t.resetAsian() } -> std::same_as<void>;
    { t.isBarrierOption() } -> std::same_as<bool>;
    { t.isKnockedOut() } -> std::same_as<bool>;
};

template<typename T>
concept RNGConcept = requires(T t) {
    { t.getNormal() } -> std::same_as<double>;
};

class MCEngine {
private:
    std::shared_ptr<SDEBase> sde;
    RNG rng;
    Payoff& payoff;

public:
    template<SDEConcept SDE, RNGConcept R, PayoffConcept P>
    MCEngine(std::shared_ptr<SDE> sde_, std::shared_ptr<R> rng_, P& payoff_)
        : sde(std::move(sde_)), rng(*rng_.get()), payoff(payoff_) {
    }

    PricingResult simulate(long NSIM, long NT, FDScheme scheme, bool antithetic = false);
};
#endif
