// This file implements the Monte Carlo engine for option pricing using a stochastic differential equation (SDE) model.
// It includes methods for simulating the price paths of the underlying asset, calculating the option payoff, and reporting the results.
// Added Asian and Barrier options support, as well as handling paths hitting zero.


#include "MCEngine.hpp"
#include <execution>
#include <vector>
#include <numeric>
#include <cmath>
#include <chrono>
#include <iostream>

PricingResult MCEngine::simulate(long NSIM, long NT, FDScheme scheme) {
    auto start = std::chrono::high_resolution_clock::now(); // Start timing

    double T = payoff.getExpiry();
    double k = T / static_cast<double>(NT);
    double sqrk = std::sqrt(k);
    double r = payoff.getRate();

    // Thread-safe accumulators
    std::vector<double> payoffs(NSIM, 0.0);
    std::vector<int> hitsZeroVec(NSIM, 0);

    // Parallelize over simulations
    std::vector<long> indices(NSIM);
    std::iota(indices.begin(), indices.end(), 0);
    std::for_each(std::execution::par, indices.begin(), indices.end(), [&](long i) {
        double S = sde->initialCondition();
        Payoff localPayoff = payoff; // True deep copy of OptionData for thread safety
        localPayoff.resetBarrier();
        localPayoff.resetAsian();
        int localHitsZero = 0;

        for (long j = 0; j < NT; ++j) {
            double dW = rng.getNormal();
            double t = j * k;
            double drift = sde->drift(t, S);
            double diffusion = sde->diffusion(t, S);

            if (scheme == FDScheme::Euler) {
                S += k * drift + sqrk * diffusion * dW;
            }
            else if (scheme == FDScheme::Milstein) {
                double diffDeriv = sde->diffusionDerivative(t, S);
                // Correct Milstein term: 0.5 * b(S) * b'(S) * [(dW)^2 - dt] = 0.5 * b(S) * b'(S) * dt * (Z^2 - 1)
                S += k * drift + sqrk * diffusion * dW + 0.5 * diffusion * diffDeriv * k * (dW * dW - 1.0);
            }
            else if (scheme == FDScheme::PredictorCorrector) {
                double S_pred = S + k * drift + sqrk * diffusion * dW;
                if (dynamic_cast<CIRModel*>(sde.get()) && S_pred < 0.0) {
                    S_pred = 0.0;
                }
                double drift_pred = sde->drift(t + k, S_pred);
                S += 0.5 * k * (drift + drift_pred) + sqrk * diffusion * dW;
            }

            if (dynamic_cast<CIRModel*>(sde.get()) && S < 0.0) {
                S = 0.0;
                localHitsZero = 1;
            }
            else if (S <= 0.0) {
                localHitsZero = 1;
            }

            localPayoff.checkBarrier(S);
            localPayoff.accumulatePrice(S);

            if (localPayoff.isBarrierOption() && localPayoff.isKnockedOut()) {
                break;
            }
        }

        double payoffVal = localPayoff(S);
        // Lockless write to disjoint vector elements for maximum parallel throughput
        payoffs[i] = payoffVal;
        hitsZeroVec[i] = localHitsZero;
        });

    // Aggregate results
    double sumPayoff = std::accumulate(payoffs.begin(), payoffs.end(), 0.0);
    double sumSquare = std::inner_product(payoffs.begin(), payoffs.end(), payoffs.begin(), 0.0);
    int hitsZero = std::accumulate(hitsZeroVec.begin(), hitsZeroVec.end(), 0);

    double discount = std::exp(-r * T);
    double price = discount * (sumPayoff / NSIM);
    double variance = (sumSquare - (sumPayoff * sumPayoff) / NSIM) / (NSIM - 1);
    double stddev = std::sqrt(variance);
    double stdErr = stddev / std::sqrt(static_cast<double>(NSIM));

    auto end = std::chrono::high_resolution_clock::now(); // End timing
    auto elapsed = std::chrono::duration<double>(end - start);

    std::cout << "Option Price: " << price << "\n"
        << "Standard Deviation: " << stddev << "\n"
        << "Standard Error: " << stdErr << "\n"
        << "Paths hitting zero: " << hitsZero << "\n"
        << "Elapsed Time: " << elapsed.count() << " seconds\n";

    return { price, stddev, stdErr, hitsZero, elapsed };
}