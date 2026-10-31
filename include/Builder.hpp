// Builder.hpp
// This is the Builder.hpp file, which provides a fluent interface for constructing option, SDE, and Monte Carlo simulation objects.
// Author: Sahil Jain

#pragma once
#include "OptionData.hpp"
#include "SDE.hpp"
#include "MCEngine.hpp"
#include <memory>
#include <tuple>
#include <string>

namespace OptionParams {
    class OptionData;
}

class OptionBuilder {
private:
    double S0;
    double K;
    double T;
    double r;
    double sig;
    double D;
    int type; // 1 for call, -1 for put
    double barrier;
    bool isUpAndOut;
    bool isBarrier;
    bool isAsian;

public:
    OptionBuilder();
    OptionBuilder& withS0(double S0_);
    OptionBuilder& withK(double K_);
    OptionBuilder& withT(double T_);
    OptionBuilder& withR(double r_);
    OptionBuilder& withSig(double sig_);
    OptionBuilder& withD(double D_);
    OptionBuilder& withType(int type_);
    OptionBuilder& withBarrier(double barrier_, bool isUpAndOut_);
    OptionBuilder& asAsian();
    OptionBuilder& resetOptionType();
    std::unique_ptr<OptionParams::OptionData> build() const;
    std::tuple<double, double, double, double, double, double, int> getParams() const;

    double getS0() const { return S0; }
    double getK() const { return K; }
    double getT() const { return T; }
    double getR() const { return r; }
    double getSig() const { return sig; }
    double getD() const { return D; }
    int getType() const { return type; }
    double getBarrier() const { return barrier; }
    bool getIsBarrier() const { return isBarrier; }
    bool getIsAsian() const { return isAsian; }
    bool getIsUpAndOut() const { return isUpAndOut; }
};

class SDEBuilder {
private:
    std::string modelType;
    double beta;
    double kappa;
    double theta;
    std::shared_ptr<OptionParams::OptionData> optionData;

public:
    SDEBuilder();
    SDEBuilder& withModelType(const std::string& type);
    SDEBuilder& withBeta(double beta_);
    SDEBuilder& withKappa(double kappa_);
    SDEBuilder& withTheta(double theta_);
    SDEBuilder& withOptionData(std::shared_ptr<OptionParams::OptionData> opt);
    std::shared_ptr<SDEBase> build() const;
    std::string getModelType() const;
    double getBeta() const { return beta; }
    double getKappa() const { return kappa; }
    double getTheta() const { return theta; }
};

class MCBuilder {
private:
    std::shared_ptr<SDEBase> sde;
    std::shared_ptr<RNG> rng;
    Payoff* payoff;
    FDScheme scheme;
    long NSIM;
    long NT;

public:
    MCBuilder();
    MCBuilder& withSDE(std::shared_ptr<SDEBase> sde_);
    MCBuilder& withRNG(std::shared_ptr<RNG> rng_);
    MCBuilder& withPayoff(Payoff* payoff_);
    MCBuilder& withScheme(FDScheme scheme_);
    MCBuilder& withNSIM(long NSIM_);
    MCBuilder& withNT(long NT_);
    std::unique_ptr<MCEngine> build() const;
    std::tuple<FDScheme, long, long> getParams() const;
    FDScheme getScheme() const { return scheme; }
    long getNSIM() const { return NSIM; }
    long getNT() const { return NT; }
};
