// Mediator.hpp
// This file defines the SimulationMediator class, which orchestrates the simulation of various option pricing models.
// Mediator will help in co ordinating the simulation process, managing configurations, and writing results to a CSV file.
// It uses builders to create option, SDE, and Monte Carlo simulation objects.
// Now we can run simulations for different option types, models, and finite difference schemes.
// And no classes will depend on each other directly, making the code more modular and maintainable.

// Author: Sahil Jain


#pragma once
#include "Builder.hpp"
#include <vector>
#include <fstream>
#include <iomanip>
#include <string>

class SimulationMediator {
private:
    std::string outputFilename;
    std::ofstream csvFile;
    std::vector<std::tuple<double, double, double, double, double, double, int>> optionConfigs;
    std::vector<std::string> modelTypes;
    std::vector<FDScheme> schemes;

    void writeHeader();
    void writeResult(const std::tuple<double, double, double, double, double, double, int>& optParams,
        const std::string& type, const std::string& model, FDScheme scheme,
        const PricingResult& result);
    void runModelSimulations(const std::tuple<double, double, double, double, double, double, int>& optParams,
        Payoff& payoff, const OptionBuilder& optBuilder, const std::string& type);

public:
    SimulationMediator(const std::string& csvFilename);
    ~SimulationMediator();
    void runSimulations();
};