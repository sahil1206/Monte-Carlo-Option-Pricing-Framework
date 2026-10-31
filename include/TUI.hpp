// TUI.hpp
// Terminal User Interface for Interactive Monte Carlo Option Pricing
// Author: Sahil Jain

#pragma once

#include "Builder.hpp"
#include "MCEngine.hpp"
#include <string>
#include <memory>
#include <cmath>
#include <numbers>

namespace Analytics {
    inline double normalCDF(double x) {
        return 0.5 * std::erfc(-x / std::sqrt(2.0));
    }

    inline double blackScholesPrice(double S0, double K, double T, double r, double sig, double D, int type) {
        if (T <= 0.0 || sig <= 0.0) {
            return std::max((type == 1 ? (S0 - K) : (K - S0)), 0.0);
        }
        double sqrtT = std::sqrt(T);
        double d1 = (std::log(S0 / K) + (r - D + 0.5 * sig * sig) * T) / (sig * sqrtT);
        double d2 = d1 - sig * sqrtT;
        if (type == 1) { // Call
            return S0 * std::exp(-D * T) * normalCDF(d1) - K * std::exp(-r * T) * normalCDF(d2);
        } else { // Put
            return K * std::exp(-r * T) * normalCDF(-d2) - S0 * std::exp(-D * T) * normalCDF(-d1);
        }
    }
}

struct TUIConfig {
    // Model Selection: "GBM", "CEV", "CIR"
    std::string modelType = "GBM";
    double cevBeta = 0.8;
    double cirKappa = 0.5;
    double cirTheta = 99.0; // 90 * 1.1

    // Contract / Payoff Selection
    std::string contractLabel = "European Call";
    int optionType = 1; // 1 = Call, -1 = Put
    bool isBarrier = false;
    double barrier = 108.0;
    bool isUpAndOut = true;
    bool isAsian = false;

    // Market & Contract Parameters
    double S0 = 90.0;
    double K = 95.0;
    double T = 0.5;
    double r = 0.03;
    double sig = 0.10;
    double D = 0.00;

    // Finite Difference Scheme
    FDScheme scheme = FDScheme::Euler;

    // Simulation / Engine Settings
    long NSIM = 10000;
    long NT = 500;
    bool fixedSeed = false;
    unsigned long seedValue = 42;
    bool antithetic = false;
};

class TUI {
private:
    TUIConfig config;
    bool hasLastResult = false;
    PricingResult lastResult;
    double lastAnalyticalPrice = 0.0;
    bool lastHasAnalytical = false;

    void drawBanner() const;
    void drawDashboard() const;
    void drawPricingCard(const PricingResult& res, double analyticalPrice, bool hasAnalytical) const;

    void menuSelectModel();
    void menuSelectContract();
    void menuEditParameters();
    void menuSelectScheme();
    void menuConfigureSimulation();
    void menuSelectPreset();
    void menuCompareSchemes();
    void menuRunBatchGrid();

    double promptDouble(const std::string& label, double currentVal, double minVal = 0.0);
    long promptLong(const std::string& label, long currentVal, long minVal = 1);
    void pauseForEnter() const;

public:
    TUI();
    void run();
    PricingResult executeSimulation(const TUIConfig& cfg, bool verbose = true);
};
