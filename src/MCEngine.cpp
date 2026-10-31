// MCEngine.cpp
// This file implements the Monte Carlo engine for option pricing using a stochastic differential equation (SDE) model.
// Supports standard and antithetic variates parallel simulation, European, Asian, and Barrier options.
// Author: Sahil Jain

#include "MCEngine.hpp"
#include <execution>
#include <vector>
#include <numeric>
#include <cmath>
#include <chrono>
#include <iostream>

PricingResult MCEngine::simulate(long NSIM, long NT, FDScheme scheme, bool antithetic) {
    auto start = std::chrono::high_resolution_clock::now(); // Start timing

    double T = payoff.getExpiry();
    double k = T / static_cast<double>(NT);
    double sqrk = std::sqrt(k);
    double r = payoff.getRate();

    auto stepSingle = [&](double& S, double dW, double t, Payoff& localP, int& hitZ) {
        double drift = sde->drift(t, S);
        double diffusion = sde->diffusion(t, S);

        if (scheme == FDScheme::Euler) {
            S += k * drift + sqrk * diffusion * dW;
        }
        else if (scheme == FDScheme::Milstein) {
            double diffDeriv = sde->diffusionDerivative(t, S);
            // Correct Milstein term: 0.5 * b(S) * b'(S) * dt * (Z^2 - 1)
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
            hitZ = 1;
        }
        else if (S <= 0.0) {
            hitZ = 1;
        }

        localP.checkBarrier(S);
        localP.accumulatePrice(S);
    };

    if (antithetic) {
        long pairs = NSIM / 2;
        if (pairs < 1) pairs = 1;

        std::vector<double> pairPayoffs(pairs, 0.0);
        std::vector<int> hitsZeroVec(pairs, 0);

        std::vector<long> indices(pairs);
        std::iota(indices.begin(), indices.end(), 0);

        std::for_each(std::execution::par, indices.begin(), indices.end(), [&](long i) {
            double S1 = sde->initialCondition();
            double S2 = sde->initialCondition();
            Payoff localPayoff1 = payoff;
            Payoff localPayoff2 = payoff;
            localPayoff1.resetBarrier(); localPayoff1.resetAsian();
            localPayoff2.resetBarrier(); localPayoff2.resetAsian();
            int hit1 = 0, hit2 = 0;

            for (long j = 0; j < NT; ++j) {
                double dW = rng.getNormal();
                double t = j * k;

                if (!localPayoff1.isKnockedOut()) {
                    stepSingle(S1, dW, t, localPayoff1, hit1);
                }
                if (!localPayoff2.isKnockedOut()) {
                    stepSingle(S2, -dW, t, localPayoff2, hit2);
                }

                if (localPayoff1.isKnockedOut() && localPayoff2.isKnockedOut()) {
                    break;
                }
            }

            pairPayoffs[i] = 0.5 * (localPayoff1(S1) + localPayoff2(S2));
            hitsZeroVec[i] = hit1 + hit2;
        });

        double sumPayoff = std::accumulate(pairPayoffs.begin(), pairPayoffs.end(), 0.0);
        double sumSquare = std::inner_product(pairPayoffs.begin(), pairPayoffs.end(), pairPayoffs.begin(), 0.0);
        int hitsZero = std::accumulate(hitsZeroVec.begin(), hitsZeroVec.end(), 0);

        double discount = std::exp(-r * T);
        double price = discount * (sumPayoff / pairs);
        double variance = (sumSquare - (sumPayoff * sumPayoff) / pairs) / (pairs - 1);
        if (variance < 0.0) variance = 0.0;
        double stddev = std::sqrt(variance);
        double stdErr = stddev / std::sqrt(static_cast<double>(pairs));

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration<double>(end - start);

        return { price, stddev, stdErr, hitsZero, elapsed };
    }
    else {
        // Standard independent paths
        std::vector<double> payoffs(NSIM, 0.0);
        std::vector<int> hitsZeroVec(NSIM, 0);

        std::vector<long> indices(NSIM);
        std::iota(indices.begin(), indices.end(), 0);

        std::for_each(std::execution::par, indices.begin(), indices.end(), [&](long i) {
            double S = sde->initialCondition();
            Payoff localPayoff = payoff;
            localPayoff.resetBarrier();
            localPayoff.resetAsian();
            int localHitsZero = 0;

            for (long j = 0; j < NT; ++j) {
                double dW = rng.getNormal();
                double t = j * k;
                stepSingle(S, dW, t, localPayoff, localHitsZero);

                if (localPayoff.isBarrierOption() && localPayoff.isKnockedOut()) {
                    break;
                }
            }

            payoffs[i] = localPayoff(S);
            hitsZeroVec[i] = localHitsZero;
        });

        double sumPayoff = std::accumulate(payoffs.begin(), payoffs.end(), 0.0);
        double sumSquare = std::inner_product(payoffs.begin(), payoffs.end(), payoffs.begin(), 0.0);
        int hitsZero = std::accumulate(hitsZeroVec.begin(), hitsZeroVec.end(), 0);

        double discount = std::exp(-r * T);
        double price = discount * (sumPayoff / NSIM);
        double variance = (sumSquare - (sumPayoff * sumPayoff) / NSIM) / (NSIM - 1);
        if (variance < 0.0) variance = 0.0;
        double stddev = std::sqrt(variance);
        double stdErr = stddev / std::sqrt(static_cast<double>(NSIM));

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration<double>(end - start);

        return { price, stddev, stdErr, hitsZero, elapsed };
    }
}
