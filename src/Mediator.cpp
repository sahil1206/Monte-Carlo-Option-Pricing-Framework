#include "Mediator.hpp"
#include <sstream>
#include <chrono>
#include <iomanip>
#include <iostream>

SimulationMediator::SimulationMediator(const std::string& csvFilename)
    : outputFilename(csvFilename) {
    csvFile.open(csvFilename);
    if (!csvFile.is_open()) {
        throw std::runtime_error("Could not open " + csvFilename + " for writing");
    }

    // Define configurations
    std::vector<double> S0_list = { 90 };
    std::vector<double> K_list = { 95 };
    std::vector<double> T_list = { 0.5, 1.0 };
    std::vector<double> r_list = { 0.03, 0.05 };
    std::vector<double> sig_list = { 0.1, 0.2 };
    std::vector<double> D_list = { 0.0, 0.05 };
    std::vector<int> type_list = { 1, -1 };

    for (double S0 : S0_list) {
        for (double K : K_list) {
            for (double T : T_list) {
                for (double r : r_list) {
                    for (double sig : sig_list) {
                        for (double D : D_list) {
                            for (int type : type_list) {
                                optionConfigs.emplace_back(S0, K, T, r, sig, D, type);
                            }
                        }
                    }
                }
            }
        }
    }

    modelTypes = { "GBM", "CEV", "CIR" };
    schemes = { FDScheme::Euler, FDScheme::Milstein, FDScheme::PredictorCorrector };
}

SimulationMediator::~SimulationMediator() {
    if (csvFile.is_open()) {
        csvFile.close();
    }
}

void SimulationMediator::writeHeader() {
    std::cout << std::fixed << std::setprecision(4);
    std::cout << std::left
        << std::setw(8) << "S0" << std::setw(8) << "K"
        << std::setw(8) << "T" << std::setw(8) << "r"
        << std::setw(8) << "sig" << std::setw(8) << "D"
        << std::setw(12) << "Type" << std::setw(8) << "Model"
        << std::setw(10) << "Scheme" << std::setw(10) << "Price"
        << std::setw(10) << "StdErr" << '\n';
    std::cout << std::string(86, '-') << '\n';

    csvFile << std::fixed << std::setprecision(4);
    csvFile << "S0,K,T,r,sig,D,Type,Model,Scheme,Price,StdDev,StdErr,HitsZero\n";
}

void SimulationMediator::writeResult(const std::tuple<double, double, double, double, double, double, int>& optParams,
    const std::string& type, const std::string& model, FDScheme scheme,
    const PricingResult& result) {
    auto [S0, K, T, r, sig, D, optType] = optParams;
    std::string schemeStr = (scheme == FDScheme::Euler ? "Euler" :
        scheme == FDScheme::Milstein ? "Milstein" : "PredCorr");

    std::cout << std::left
        << std::setw(8) << S0
        << std::setw(8) << K
        << std::setw(8) << T
        << std::setw(8) << r
        << std::setw(8) << sig
        << std::setw(8) << D
        << std::setw(12) << type
        << std::setw(8) << model
        << std::setw(10) << schemeStr
        << std::setw(10) << result.price
        << std::setw(10) << result.stdError
        << '\n';

    csvFile << S0 << "," << K << "," << T << "," << r << "," << sig << "," << D << ","
        << type << "," << model << "," << schemeStr << ","
        << result.price << "," << result.stdDev << "," << result.stdError << ","
        << result.hitsZero << "\n";
}

void SimulationMediator::runModelSimulations(const std::tuple<double, double, double, double, double, double, int>& optParams,
    Payoff& payoff, const OptionBuilder& optBuilder, const std::string& type) {
    auto [S0, K, T, r, sig, D, optType] = optParams;
    std::shared_ptr<RNG> rng = std::make_shared<RNG>();

    for (const auto& model : modelTypes) {
        SDEBuilder sdeBuilder;
        sdeBuilder.withModelType(model).withOptionData(std::shared_ptr<OptionParams::OptionData>(optBuilder.build()));

        if (model == "CEV") {
            sdeBuilder.withBeta(0.8);
        }
        else if (model == "CIR") {
            sdeBuilder.withKappa(0.5).withTheta(S0 * 1.1);
        }

        auto sde = sdeBuilder.build();

        for (const auto& scheme : schemes) {
            MCBuilder mcBuilder;
            mcBuilder.withSDE(sde).withRNG(rng).withPayoff(&payoff).withScheme(scheme).withNSIM(10000).withNT(500);
            auto mc = mcBuilder.build();
            std::ostringstream buffer;
            std::streambuf* oldCout = std::cout.rdbuf(buffer.rdbuf());
            PricingResult result = mc->simulate(10000, 500, scheme);
            std::cout.rdbuf(oldCout);
            writeResult(optParams, type, model, scheme, result);
        }
    }
}

void SimulationMediator::runSimulations() {
    writeHeader();

    for (const auto& optParams : optionConfigs) {
        OptionBuilder optBuilder;
        auto [S0, K, T, r, sig, D, type] = optParams;
        optBuilder.withS0(S0).withK(K).withT(T).withR(r).withSig(sig).withD(D).withType(type);

        // European
        auto optData = optBuilder.build();
        Payoff payoff(*optData);
        runModelSimulations(optParams, payoff, optBuilder, type == 1 ? "Call" : "Put");

        // Barrier (Up-and-Out Call for calls only)
        if (type == 1) {
            optBuilder.withBarrier(S0 * 1.2, true);
            auto barrierOpt = optBuilder.build();
            Payoff barrierPayoff(*barrierOpt);
            runModelSimulations(optParams, barrierPayoff, optBuilder, "UpOutCall");
            optBuilder = OptionBuilder().withS0(S0).withK(K).withT(T).withR(r).withSig(sig).withD(D).withType(type);
        }

        // Asian
        optBuilder.asAsian();
        auto asianOpt = optBuilder.build();
        Payoff asianPayoff(*asianOpt);
        runModelSimulations(optParams, asianPayoff, optBuilder, type == 1 ? "AsianCall" : "AsianPut");
    }

    std::cout << "\nResults successfully written to " << outputFilename << "\n";
}