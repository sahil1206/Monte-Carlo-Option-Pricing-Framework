// TUI.cpp
// Interactive Terminal User Interface implementation for Monte Carlo Option Pricing
// Author: Sahil Jain

#include "TUI.hpp"
#include "Mediator.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <limits>

#ifdef _WIN32
#include <windows.h>
static void initTerminal() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
}
#else
static void initTerminal() {}
#endif

// ANSI Escape Color Codes
namespace Color {
    const std::string Reset   = "\033[0m";
    const std::string Bold    = "\033[1m";
    const std::string Dim     = "\033[2m";
    const std::string Cyan    = "\033[36m";
    const std::string Green   = "\033[32m";
    const std::string Yellow  = "\033[33m";
    const std::string Magenta = "\033[35m";
    const std::string Blue    = "\033[34m";
    const std::string Red     = "\033[31m";
    const std::string Gray    = "\033[90m";
}

TUI::TUI() {
    initTerminal();
}

void TUI::drawBanner() const {
    std::cout << Color::Cyan << Color::Bold;
    std::cout << "================================================================================\n";
    std::cout << "     MONTE CARLO SIMULATION ENGINE - QUANTITATIVE PRICING WORKBENCH (C++20)     \n";
    std::cout << "================================================================================\n";
    std::cout << Color::Reset;
}

void TUI::drawDashboard() const {
    std::cout << Color::Bold << Color::Yellow << "  [ACTIVE CONFIGURATION]\n" << Color::Reset;
    std::cout << "  * " << Color::Bold << "Asset Model:       " << Color::Reset << Color::Green << config.modelType << Color::Reset;
    if (config.modelType == "CEV") {
        std::cout << Color::Gray << " (Elasticity beta = " << std::fixed << std::setprecision(2) << config.cevBeta << ")" << Color::Reset;
    } else if (config.modelType == "CIR") {
        std::cout << Color::Gray << " (kappa = " << std::fixed << std::setprecision(2) << config.cirKappa 
                  << ", theta = " << config.cirTheta << ")" << Color::Reset;
    }
    std::cout << "\n";

    std::cout << "  * " << Color::Bold << "Contract Type:     " << Color::Reset << Color::Green << config.contractLabel << Color::Reset;
    if (config.isBarrier) {
        std::cout << Color::Gray << " (Barrier = " << std::fixed << std::setprecision(2) << config.barrier 
                  << ", " << (config.isUpAndOut ? "Up-and-Out" : "Down-and-Out") << ")" << Color::Reset;
    }
    std::cout << "\n";

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "  * " << Color::Bold << "Market Params:     " << Color::Reset
              << "S0 = " << Color::Cyan << config.S0 << Color::Reset << "  |  "
              << "K = " << Color::Cyan << config.K << Color::Reset << "  |  "
              << "T = " << Color::Cyan << config.T << " yrs" << Color::Reset << "\n";
    std::cout << "                       "
              << "r = " << Color::Cyan << config.r << Color::Reset << "  |  "
              << "sig = " << Color::Cyan << config.sig << Color::Reset << "  |  "
              << "D = " << Color::Cyan << config.D << Color::Reset << "\n";

    std::string schemeStr = (config.scheme == FDScheme::Euler ? "Euler-Maruyama (Order 0.5)" :
                            (config.scheme == FDScheme::Milstein ? "Milstein Scheme (Order 1.0)" :
                                                                   "Predictor-Corrector (Two-Step Stabilized)"));
    std::cout << "  * " << Color::Bold << "Numerical Scheme:   " << Color::Reset << Color::Magenta << schemeStr << Color::Reset << "\n";

    std::cout << "  * " << Color::Bold << "Simulation Paths:   " << Color::Reset 
              << "NSIM = " << Color::Cyan << config.NSIM << Color::Reset << "  |  "
              << "NT = " << Color::Cyan << config.NT << " steps" << Color::Reset << "\n";

    std::cout << "  * " << Color::Bold << "RNG & Variates:    " << Color::Reset
              << "mt19937_64 (" << (config.fixedSeed ? ("Fixed Seed = " + std::to_string(config.seedValue)) : "Thread-Local Hardware Clock") << ")"
              << (config.antithetic ? Color::Green + " [Antithetic Variates ON]" + Color::Reset : Color::Gray + " [Standard Random]" + Color::Reset) << "\n";

    std::cout << Color::Gray << "--------------------------------------------------------------------------------\n" << Color::Reset;

    if (hasLastResult) {
        drawPricingCard(lastResult, lastAnalyticalPrice, lastHasAnalytical);
        std::cout << Color::Gray << "--------------------------------------------------------------------------------\n" << Color::Reset;
    }

    std::cout << Color::Bold << Color::Yellow << "  [INTERACTIVE MENU]\n" << Color::Reset;
    std::cout << "   " << Color::Bold << "1." << Color::Reset << " Run Pricing Simulation with Current Settings\n";
    std::cout << "   " << Color::Bold << "2." << Color::Reset << " Select Asset Dynamics Model (GBM / CEV / CIR)\n";
    std::cout << "   " << Color::Bold << "3." << Color::Reset << " Select Option Contract & Payoff Type\n";
    std::cout << "   " << Color::Bold << "4." << Color::Reset << " Edit Market & Contract Parameters (S0, K, T, r, sig, D, Barrier)\n";
    std::cout << "   " << Color::Bold << "5." << Color::Reset << " Select Finite Difference Scheme (Euler / Milstein / Pred-Corr)\n";
    std::cout << "   " << Color::Bold << "6." << Color::Reset << " Configure Simulation Settings (Paths, Steps, Seed, Antithetic)\n";
    std::cout << "   " << Color::Bold << "7." << Color::Reset << " Load Standard Market Scenario Presets\n";
    std::cout << "   " << Color::Bold << "8." << Color::Reset << " Compare All Numerical Schemes Side-by-Side\n";
    std::cout << "   " << Color::Bold << "9." << Color::Reset << " Run Full Batch Grid Matrix & Export to CSV\n";
    std::cout << "   " << Color::Bold << "0." << Color::Reset << " Exit Workbench\n";
    std::cout << Color::Cyan << "================================================================================\n" << Color::Reset;
}

void TUI::drawPricingCard(const PricingResult& res, double analyticalPrice, bool hasAnalytical) const {
    double ci95_lower = res.price - 1.95996 * res.stdError;
    double ci95_upper = res.price + 1.95996 * res.stdError;
    double ci99_lower = res.price - 2.57583 * res.stdError;
    double ci99_upper = res.price + 2.57583 * res.stdError;
    double ms = res.elapsedTime.count() * 1000.0;
    double throughput = (res.elapsedTime.count() > 0.0) ? (config.NSIM / res.elapsedTime.count()) : 0.0;

    std::cout << Color::Bold << Color::Green << "  [LATEST SIMULATION RESULTS]\n" << Color::Reset;
    std::cout << std::fixed << std::setprecision(5);
    std::cout << "   * " << Color::Bold << "Monte Carlo Price:       " << Color::Reset << Color::Green << Color::Bold << res.price << Color::Reset << "\n";
    std::cout << "   * " << Color::Bold << "Standard Error (SE):     " << Color::Reset << res.stdError << "\n";
    std::cout << "   * " << Color::Bold << "95% Confidence Interval: " << Color::Reset << "[" << ci95_lower << ", " << ci95_upper << "]\n";
    std::cout << "   * " << Color::Bold << "99% Confidence Interval: " << Color::Reset << "[" << ci99_lower << ", " << ci99_upper << "]\n";
    std::cout << "   * " << Color::Bold << "Sample Standard Dev:     " << Color::Reset << res.stdDev << "\n";
    std::cout << "   * " << Color::Bold << "Boundary Hits (S <= 0):  " << Color::Reset << res.hitsZero << " / " << config.NSIM << "\n";
    std::cout << "   * " << Color::Bold << "Runtime & Throughput:    " << Color::Reset << std::fixed << std::setprecision(2) 
              << ms << " ms (" << res.elapsedTime.count() << " s) | " << std::fixed << std::setprecision(0) << throughput << " paths/sec\n";

    if (hasAnalytical) {
        double absErr = std::abs(res.price - analyticalPrice);
        double relErr = (analyticalPrice != 0.0) ? (absErr / analyticalPrice * 100.0) : 0.0;
        bool in95 = (absErr <= 1.95996 * res.stdError);

        std::cout << std::fixed << std::setprecision(5);
        std::cout << "   * " << Color::Bold << "Black-Scholes Exact:     " << Color::Reset << Color::Cyan << analyticalPrice << Color::Reset << "\n";
        std::cout << "   * " << Color::Bold << "Absolute Error |MC - BS|:" << Color::Reset << " " << absErr 
                  << " (" << std::fixed << std::setprecision(2) << relErr << "%)\n";
        std::cout << "   * " << Color::Bold << "Analytical Coverage:     " << Color::Reset 
                  << (in95 ? (Color::Green + "PASSED (Within 95% Confidence Interval)" + Color::Reset) : 
                             (Color::Yellow + "Slight deviation outside 95% CI (Normal variance)" + Color::Reset)) << "\n";
    }
}

PricingResult TUI::executeSimulation(const TUIConfig& cfg, bool verbose) {
    OptionBuilder optBuilder;
    optBuilder.withS0(cfg.S0)
              .withK(cfg.K)
              .withT(cfg.T)
              .withR(cfg.r)
              .withSig(cfg.sig)
              .withD(cfg.D)
              .withType(cfg.optionType);

    if (cfg.isBarrier) {
        optBuilder.withBarrier(cfg.barrier, cfg.isUpAndOut);
    } else if (cfg.isAsian) {
        optBuilder.asAsian();
    } else {
        optBuilder.resetOptionType();
    }

    auto optData = optBuilder.build();
    Payoff payoff(*optData);

    SDEBuilder sdeBuilder;
    sdeBuilder.withModelType(cfg.modelType)
              .withOptionData(std::shared_ptr<OptionParams::OptionData>(optBuilder.build()));

    if (cfg.modelType == "CEV") {
        sdeBuilder.withBeta(cfg.cevBeta);
    } else if (cfg.modelType == "CIR") {
        sdeBuilder.withKappa(cfg.cirKappa)
                  .withTheta(cfg.cirTheta);
    }

    auto sde = sdeBuilder.build();
    std::shared_ptr<RNG> rng = std::make_shared<RNG>();

    if (cfg.fixedSeed) {
        RNG::setSeed(cfg.seedValue);
    } else {
        RNG::setRandomSeed();
    }

    MCBuilder mcBuilder;
    mcBuilder.withSDE(sde)
             .withRNG(rng)
             .withPayoff(&payoff)
             .withScheme(cfg.scheme)
             .withNSIM(cfg.NSIM)
             .withNT(cfg.NT);

    auto engine = mcBuilder.build();

    if (verbose) {
        std::cout << Color::Gray << "Simulating " << cfg.NSIM << " paths across " << cfg.NT << " time steps with multithreading...\n" << Color::Reset;
    }

    return engine->simulate(cfg.NSIM, cfg.NT, cfg.scheme, cfg.antithetic);
}

double TUI::promptDouble(const std::string& label, double currentVal, double minVal) {
    while (true) {
        std::cout << "  " << label << " [" << Color::Cyan << currentVal << Color::Reset << "]: ";
        std::string line;
        std::getline(std::cin, line);
        if (line.empty()) {
            return currentVal;
        }
        try {
            double val = std::stod(line);
            if (val >= minVal) {
                return val;
            }
            std::cout << Color::Red << "  Error: Value must be >= " << minVal << ". Try again.\n" << Color::Reset;
        } catch (...) {
            std::cout << Color::Red << "  Error: Invalid number format. Try again.\n" << Color::Reset;
        }
    }
}

long TUI::promptLong(const std::string& label, long currentVal, long minVal) {
    while (true) {
        std::cout << "  " << label << " [" << Color::Cyan << currentVal << Color::Reset << "]: ";
        std::string line;
        std::getline(std::cin, line);
        if (line.empty()) {
            return currentVal;
        }
        try {
            long val = std::stol(line);
            if (val >= minVal) {
                return val;
            }
            std::cout << Color::Red << "  Error: Value must be >= " << minVal << ". Try again.\n" << Color::Reset;
        } catch (...) {
            std::cout << Color::Red << "  Error: Invalid integer. Try again.\n" << Color::Reset;
        }
    }
}

void TUI::pauseForEnter() const {
    std::cout << Color::Dim << "\nPress Enter to continue..." << Color::Reset;
    std::string dummy;
    std::getline(std::cin, dummy);
}

void TUI::menuSelectModel() {
    std::cout << "\n" << Color::Bold << Color::Yellow << "--- Select Asset Dynamics Model ---" << Color::Reset << "\n";
    std::cout << "  1. Geometric Brownian Motion (GBM) [Black-Scholes log-normal]\n";
    std::cout << "  2. Constant Elasticity of Variance (CEV) [Leverage & volatility skew]\n";
    std::cout << "  3. Cox-Ingersoll-Ross (CIR) [Mean-reverting square-root process]\n";
    std::cout << "  0. Return to main menu\n";
    std::cout << "Choice [1-3, 0 to cancel]: ";

    std::string choice;
    std::getline(std::cin, choice);

    if (choice == "1") {
        config.modelType = "GBM";
        std::cout << Color::Green << "Model updated to GBM.\n" << Color::Reset;
    } else if (choice == "2") {
        config.modelType = "CEV";
        config.cevBeta = promptDouble("Enter CEV elasticity beta (e.g. 0.8)", config.cevBeta, 0.01);
        std::cout << Color::Green << "Model updated to CEV (beta = " << config.cevBeta << ").\n" << Color::Reset;
    } else if (choice == "3") {
        config.modelType = "CIR";
        config.cirKappa = promptDouble("Enter CIR mean-reversion speed kappa", config.cirKappa, 0.001);
        config.cirTheta = promptDouble("Enter CIR long-term mean theta", config.cirTheta, 0.001);
        std::cout << Color::Green << "Model updated to CIR (kappa = " << config.cirKappa << ", theta = " << config.cirTheta << ").\n" << Color::Reset;
    }
}

void TUI::menuSelectContract() {
    std::cout << "\n" << Color::Bold << Color::Yellow << "--- Select Option Contract & Payoff Type ---" << Color::Reset << "\n";
    std::cout << "  1. European Plain Vanilla Call\n";
    std::cout << "  2. European Plain Vanilla Put\n";
    std::cout << "  3. Up-and-Out Barrier Call\n";
    std::cout << "  4. Arithmetic Asian Call (Average Price)\n";
    std::cout << "  5. Arithmetic Asian Put (Average Price)\n";
    std::cout << "  0. Return to main menu\n";
    std::cout << "Choice [1-5, 0 to cancel]: ";

    std::string choice;
    std::getline(std::cin, choice);

    if (choice == "1") {
        config.contractLabel = "European Call";
        config.optionType = 1;
        config.isBarrier = false;
        config.isAsian = false;
        std::cout << Color::Green << "Contract updated to European Call.\n" << Color::Reset;
    } else if (choice == "2") {
        config.contractLabel = "European Put";
        config.optionType = -1;
        config.isBarrier = false;
        config.isAsian = false;
        std::cout << Color::Green << "Contract updated to European Put.\n" << Color::Reset;
    } else if (choice == "3") {
        config.contractLabel = "Up-and-Out Barrier Call";
        config.optionType = 1;
        config.isBarrier = true;
        config.isUpAndOut = true;
        config.isAsian = false;
        config.barrier = promptDouble("Enter Up-and-Out Barrier Level (e.g. 108.0)", config.barrier, config.S0);
        std::cout << Color::Green << "Contract updated to Up-and-Out Barrier Call (B = " << config.barrier << ").\n" << Color::Reset;
    } else if (choice == "4") {
        config.contractLabel = "Asian Arithmetic Call";
        config.optionType = 1;
        config.isBarrier = false;
        config.isAsian = true;
        std::cout << Color::Green << "Contract updated to Asian Arithmetic Call.\n" << Color::Reset;
    } else if (choice == "5") {
        config.contractLabel = "Asian Arithmetic Put";
        config.optionType = -1;
        config.isBarrier = false;
        config.isAsian = true;
        std::cout << Color::Green << "Contract updated to Asian Arithmetic Put.\n" << Color::Reset;
    }
}

void TUI::menuEditParameters() {
    std::cout << "\n" << Color::Bold << Color::Yellow << "--- Edit Market & Contract Parameters ---" << Color::Reset << "\n";
    std::cout << Color::Dim << "Press Enter on any field to preserve the existing value.\n" << Color::Reset;

    config.S0  = promptDouble("Initial Spot Price (S0)", config.S0, 0.001);
    config.K   = promptDouble("Strike Price (K)", config.K, 0.001);
    config.T   = promptDouble("Maturity Expiry in Years (T)", config.T, 0.001);
    config.r   = promptDouble("Risk-free Interest Rate (r, e.g. 0.03 for 3%)", config.r, -0.50);
    config.sig = promptDouble("Volatility sigma (e.g. 0.20 for 20%)", config.sig, 0.0001);
    config.D   = promptDouble("Continuous Dividend Yield (D, e.g. 0.0)", config.D, 0.0);

    if (config.isBarrier) {
        config.barrier = promptDouble("Barrier Level", config.barrier, 0.001);
    }

    if (config.modelType == "CEV") {
        config.cevBeta = promptDouble("CEV Elasticity Parameter beta", config.cevBeta, 0.01);
    } else if (config.modelType == "CIR") {
        config.cirKappa = promptDouble("CIR Mean Reversion Speed kappa", config.cirKappa, 0.001);
        config.cirTheta = promptDouble("CIR Long-Term Mean theta", config.cirTheta, 0.001);
    }

    std::cout << Color::Green << "Parameters successfully updated.\n" << Color::Reset;
}

void TUI::menuSelectScheme() {
    std::cout << "\n" << Color::Bold << Color::Yellow << "--- Select Finite Difference Scheme ---" << Color::Reset << "\n";
    std::cout << "  1. Euler-Maruyama Scheme (Standard Strong Order 0.5)\n";
    std::cout << "  2. Milstein Scheme (Strong Order 1.0 with analytical spatial diffusion derivative)\n";
    std::cout << "  3. Drift-Adjusted Predictor-Corrector Scheme (Two-step stabilized drift averaging)\n";
    std::cout << "  0. Return to main menu\n";
    std::cout << "Choice [1-3, 0 to cancel]: ";

    std::string choice;
    std::getline(std::cin, choice);

    if (choice == "1") {
        config.scheme = FDScheme::Euler;
        std::cout << Color::Green << "Scheme set to Euler-Maruyama.\n" << Color::Reset;
    } else if (choice == "2") {
        config.scheme = FDScheme::Milstein;
        std::cout << Color::Green << "Scheme set to Milstein.\n" << Color::Reset;
    } else if (choice == "3") {
        config.scheme = FDScheme::PredictorCorrector;
        std::cout << Color::Green << "Scheme set to Predictor-Corrector.\n" << Color::Reset;
    }
}

void TUI::menuConfigureSimulation() {
    std::cout << "\n" << Color::Bold << Color::Yellow << "--- Configure Simulation & RNG Settings ---" << Color::Reset << "\n";
    config.NSIM = promptLong("Number of Simulation Paths (NSIM)", config.NSIM, 100);
    config.NT   = promptLong("Number of Time Steps (NT)", config.NT, 10);

    std::cout << "  Enable Antithetic Variates for Variance Reduction? (y/n) [" 
              << (config.antithetic ? "y" : "n") << "]: ";
    std::string antiStr;
    std::getline(std::cin, antiStr);
    if (!antiStr.empty()) {
        config.antithetic = (antiStr == "y" || antiStr == "Y" || antiStr == "yes");
    }

    std::cout << "  RNG Seed Mode: (1 = Hardware Clock ^ ThreadID [Random], 2 = Fixed Seed [Reproducible]) ["
              << (config.fixedSeed ? "2" : "1") << "]: ";
    std::string seedMode;
    std::getline(std::cin, seedMode);
    if (seedMode == "2") {
        config.fixedSeed = true;
        config.seedValue = static_cast<unsigned long>(promptLong("Enter Fixed Seed Value", config.seedValue, 1));
    } else if (seedMode == "1") {
        config.fixedSeed = false;
    }

    std::cout << Color::Green << "Simulation settings successfully updated.\n" << Color::Reset;
}

void TUI::menuSelectPreset() {
    std::cout << "\n" << Color::Bold << Color::Yellow << "--- Load Scenario Presets ---" << Color::Reset << "\n";
    std::cout << "  1. Benchmark At-the-Money European Call (S0=100, K=100, T=1.0, r=5%, sig=20%)\n";
    std::cout << "  2. Volatility Skew Test under CEV Model (beta=0.60, S0=90, K=95, sig=20%)\n";
    std::cout << "  3. Mean-Reverting Short Rate under CIR Model (kappa=1.2, theta=0.04, S0=0.03)\n";
    std::cout << "  4. Up-and-Out Barrier Knockout Stress Test (S0=100, K=100, Barrier=115)\n";
    std::cout << "  5. Path-Dependent Arithmetic Asian Average Call (S0=90, K=95, T=0.5)\n";
    std::cout << "  0. Return to main menu\n";
    std::cout << "Choice [1-5, 0 to cancel]: ";

    std::string choice;
    std::getline(std::cin, choice);

    if (choice == "1") {
        config.modelType = "GBM";
        config.contractLabel = "European Call";
        config.optionType = 1;
        config.isBarrier = false;
        config.isAsian = false;
        config.S0 = 100.0; config.K = 100.0; config.T = 1.0; config.r = 0.05; config.sig = 0.20; config.D = 0.0;
        config.scheme = FDScheme::Euler;
        std::cout << Color::Green << "Loaded: Benchmark ATM European Call.\n" << Color::Reset;
    } else if (choice == "2") {
        config.modelType = "CEV";
        config.cevBeta = 0.60;
        config.contractLabel = "European Call";
        config.optionType = 1;
        config.isBarrier = false;
        config.isAsian = false;
        config.S0 = 90.0; config.K = 95.0; config.T = 0.5; config.r = 0.03; config.sig = 0.20; config.D = 0.0;
        config.scheme = FDScheme::Milstein;
        std::cout << Color::Green << "Loaded: Volatility Skew CEV Model with Milstein.\n" << Color::Reset;
    } else if (choice == "3") {
        config.modelType = "CIR";
        config.cirKappa = 1.20;
        config.cirTheta = 0.04;
        config.contractLabel = "European Call";
        config.optionType = 1;
        config.isBarrier = false;
        config.isAsian = false;
        config.S0 = 0.03; config.K = 0.035; config.T = 1.0; config.r = 0.03; config.sig = 0.08; config.D = 0.0;
        config.scheme = FDScheme::PredictorCorrector;
        std::cout << Color::Green << "Loaded: Mean-Reverting Short Rate under CIR Model.\n" << Color::Reset;
    } else if (choice == "4") {
        config.modelType = "GBM";
        config.contractLabel = "Up-and-Out Barrier Call";
        config.optionType = 1;
        config.isBarrier = true;
        config.isUpAndOut = true;
        config.barrier = 115.0;
        config.isAsian = false;
        config.S0 = 100.0; config.K = 100.0; config.T = 0.5; config.r = 0.05; config.sig = 0.20; config.D = 0.0;
        config.scheme = FDScheme::Euler;
        std::cout << Color::Green << "Loaded: Up-and-Out Barrier Knockout Stress Test.\n" << Color::Reset;
    } else if (choice == "5") {
        config.modelType = "GBM";
        config.contractLabel = "Asian Arithmetic Call";
        config.optionType = 1;
        config.isBarrier = false;
        config.isAsian = true;
        config.S0 = 90.0; config.K = 95.0; config.T = 0.5; config.r = 0.03; config.sig = 0.15; config.D = 0.0;
        config.scheme = FDScheme::Euler;
        std::cout << Color::Green << "Loaded: Path-Dependent Arithmetic Asian Call.\n" << Color::Reset;
    }
}

void TUI::menuCompareSchemes() {
    std::cout << "\n" << Color::Bold << Color::Yellow << "=== Comparing Numerical Schemes Side-by-Side ===" << Color::Reset << "\n";
    std::cout << Color::Gray << "Running Euler, Milstein, and Predictor-Corrector across " 
              << config.NSIM << " paths with identical parameters...\n\n" << Color::Reset;

    TUIConfig cfgEuler = config; cfgEuler.scheme = FDScheme::Euler;
    TUIConfig cfgMilst = config; cfgMilst.scheme = FDScheme::Milstein;
    TUIConfig cfgPredC = config; cfgPredC.scheme = FDScheme::PredictorCorrector;

    auto resEuler = executeSimulation(cfgEuler, false);
    auto resMilst = executeSimulation(cfgMilst, false);
    auto resPredC = executeSimulation(cfgPredC, false);

    bool hasAnalytic = (!config.isBarrier && !config.isAsian && config.modelType == "GBM");
    double bsPrice = 0.0;
    if (hasAnalytic) {
        bsPrice = Analytics::blackScholesPrice(config.S0, config.K, config.T, config.r, config.sig, config.D, config.optionType);
    }

    std::cout << Color::Bold << Color::Cyan;
    std::cout << std::left
              << std::setw(20) << "Scheme"
              << std::setw(12) << "MC Price"
              << std::setw(12) << "Std Error"
              << std::setw(22) << "95% Confidence Int"
              << std::setw(12) << "Zero Hits"
              << std::setw(12) << "Time (ms)" << "\n";
    std::cout << std::string(90, '-') << "\n" << Color::Reset;

    auto printRow = [&](const std::string& name, const PricingResult& res) {
        double lower = res.price - 1.95996 * res.stdError;
        double upper = res.price + 1.95996 * res.stdError;
        std::ostringstream ci;
        ci << std::fixed << std::setprecision(4) << "[" << lower << ", " << upper << "]";

        std::cout << std::left << std::fixed << std::setprecision(4)
                  << std::setw(20) << name
                  << std::setw(12) << res.price
                  << std::setw(12) << res.stdError
                  << std::setw(22) << ci.str()
                  << std::setw(12) << res.hitsZero
                  << std::setw(12) << std::setprecision(2) << (res.elapsedTime.count() * 1000.0)
                  << "\n";
    };

    printRow("Euler-Maruyama", resEuler);
    printRow("Milstein", resMilst);
    printRow("Predictor-Corrector", resPredC);

    if (hasAnalytic) {
        std::cout << std::string(90, '-') << "\n";
        std::cout << Color::Bold << Color::Green 
                  << "Exact Analytical Black-Scholes Price: " << std::fixed << std::setprecision(5) << bsPrice 
                  << Color::Reset << "\n";
    }

    pauseForEnter();
}

void TUI::menuRunBatchGrid() {
    std::cout << "\n" << Color::Bold << Color::Yellow << "=== Running Comprehensive Batch Grid Simulation ===" << Color::Reset << "\n";
    std::cout << "This will run all parameter permutations (S0, K, T, r, sig, D, Calls/Puts/Barriers/Asians, GBM/CEV/CIR, Euler/Milstein/PredCorr)\n";
    std::cout << "and stream formatted results to both the console and a timestamped CSV file.\n\n";
    std::cout << "Proceed? (y/n) [y]: ";

    std::string confirm;
    std::getline(std::cin, confirm);
    if (!confirm.empty() && confirm != "y" && confirm != "Y" && confirm != "yes") {
        std::cout << "Batch simulation cancelled.\n";
        return;
    }

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

    auto start = std::chrono::high_resolution_clock::now();
    try {
        SimulationMediator mediator(csvFilename);
        mediator.runSimulations();
    } catch (const std::exception& e) {
        std::cout << Color::Red << "Error during batch execution: " << e.what() << "\n" << Color::Reset;
        pauseForEnter();
        return;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto totalElapsed = std::chrono::duration<double>(end - start);

    std::cout << Color::Green << Color::Bold << "\nBatch simulation completed in " 
              << totalElapsed.count() << " seconds.\n" << Color::Reset;
    std::cout << "Results saved to: " << Color::Cyan << csvFilename << Color::Reset << "\n";
    pauseForEnter();
}

void TUI::run() {
    while (true) {
        drawBanner();
        drawDashboard();

        std::cout << "Enter option [0-9]: ";
        std::string input;
        std::getline(std::cin, input);

        if (input.empty()) {
            continue;
        }

        if (input == "0" || input == "exit" || input == "q") {
            std::cout << "\nExiting Monte Carlo Pricing Workbench. Goodbye!\n";
            break;
        }
        else if (input == "1") {
            // Run simulation
            std::cout << "\n" << Color::Bold << "Executing Simulation..." << Color::Reset << "\n";
            lastResult = executeSimulation(config, true);
            hasLastResult = true;

            lastHasAnalytical = (!config.isBarrier && !config.isAsian && config.modelType == "GBM");
            if (lastHasAnalytical) {
                lastAnalyticalPrice = Analytics::blackScholesPrice(config.S0, config.K, config.T, config.r, config.sig, config.D, config.optionType);
            }
            std::cout << Color::Green << "\nSimulation Complete!\n" << Color::Reset;
            pauseForEnter();
        }
        else if (input == "2") {
            menuSelectModel();
        }
        else if (input == "3") {
            menuSelectContract();
        }
        else if (input == "4") {
            menuEditParameters();
        }
        else if (input == "5") {
            menuSelectScheme();
        }
        else if (input == "6") {
            menuConfigureSimulation();
        }
        else if (input == "7") {
            menuSelectPreset();
        }
        else if (input == "8") {
            menuCompareSchemes();
        }
        else if (input == "9") {
            menuRunBatchGrid();
        }
        else {
            std::cout << Color::Red << "Invalid option. Please enter a number between 0 and 9.\n" << Color::Reset;
        }
    }
}
