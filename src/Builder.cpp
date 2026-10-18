#include "Builder.hpp"

OptionBuilder::OptionBuilder()
    : S0(90.0), K(95.0), T(0.5), r(0.03), sig(0.1), D(0.0), type(1),
    barrier(0.0), isUpAndOut(false), isBarrier(false), isAsian(false) {
}

OptionBuilder& OptionBuilder::withS0(double S0_) {
    S0 = S0_;
    return *this;
}

OptionBuilder& OptionBuilder::withK(double K_) {
    K = K_;
    return *this;
}

OptionBuilder& OptionBuilder::withT(double T_) {
    T = T_;
    return *this;
}

OptionBuilder& OptionBuilder::withR(double r_) {
    r = r_;
    return *this;
}

OptionBuilder& OptionBuilder::withSig(double sig_) {
    sig = sig_;
    return *this;
}

OptionBuilder& OptionBuilder::withD(double D_) {
    D = D_;
    return *this;
}

OptionBuilder& OptionBuilder::withType(int type_) {
    type = type_;
    return *this;
}

OptionBuilder& OptionBuilder::withBarrier(double barrier_, bool isUpAndOut_) {
    barrier = barrier_;
    isUpAndOut = isUpAndOut_;
    isBarrier = true;
    return *this;
}

OptionBuilder& OptionBuilder::asAsian() {
    isAsian = true;
    return *this;
}

std::unique_ptr<OptionParams::OptionData> OptionBuilder::build() const {
    if (isBarrier) {
        return std::make_unique<OptionParams::BarrierOptionData>(S0, K, T, r, sig, D, type, barrier, isUpAndOut);
    }
    else if (isAsian) {
        return std::make_unique<OptionParams::AsianOptionData>(S0, K, T, r, sig, D, type);
    }
    return std::make_unique<OptionParams::OptionData>(S0, K, T, r, sig, D, type);
}

std::tuple<double, double, double, double, double, double, int> OptionBuilder::getParams() const {
    return std::make_tuple(S0, K, T, r, sig, D, type);
}

SDEBuilder::SDEBuilder()
    : modelType("GBM"), beta(0.8), kappa(0.5), theta(90.0 * 1.1), optionData(nullptr) {
}

SDEBuilder& SDEBuilder::withModelType(const std::string& type) {
    modelType = type;
    return *this;
}

SDEBuilder& SDEBuilder::withBeta(double beta_) {
    beta = beta_;
    return *this;
}

SDEBuilder& SDEBuilder::withKappa(double kappa_) {
    kappa = kappa_;
    return *this;
}

SDEBuilder& SDEBuilder::withTheta(double theta_) {
    theta = theta_;
    return *this;
}

SDEBuilder& SDEBuilder::withOptionData(std::shared_ptr<OptionParams::OptionData> opt) {
    optionData = std::move(opt);
    return *this;
}

std::shared_ptr<SDEBase> SDEBuilder::build() const {
    if (!optionData) {
        throw std::runtime_error("Option data not set for SDEBuilder");
    }
    if (modelType == "CEV") {
        return std::make_shared<CEVModel>(*optionData, beta);
    }
    else if (modelType == "CIR") {
        return std::make_shared<CIRModel>(*optionData, kappa, theta);
    }
    return std::make_shared<GBMModel>(*optionData);
}

std::string SDEBuilder::getModelType() const {
    return modelType;
}

MCBuilder::MCBuilder()
    : sde(nullptr), rng(nullptr), payoff(nullptr), scheme(FDScheme::Euler), NSIM(10000), NT(500) {
}

MCBuilder& MCBuilder::withSDE(std::shared_ptr<SDEBase> sde_) {
    sde = std::move(sde_);
    return *this;
}

MCBuilder& MCBuilder::withRNG(std::shared_ptr<RNG> rng_) {
    rng = std::move(rng_);
    return *this;
}

MCBuilder& MCBuilder::withPayoff(Payoff* payoff_) {
    payoff = payoff_;
    return *this;
}

MCBuilder& MCBuilder::withScheme(FDScheme scheme_) {
    scheme = scheme_;
    return *this;
}

MCBuilder& MCBuilder::withNSIM(long NSIM_) {
    NSIM = NSIM_;
    return *this;
}

MCBuilder& MCBuilder::withNT(long NT_) {
    NT = NT_;
    return *this;
}

std::unique_ptr<MCEngine> MCBuilder::build() const {
    if (!sde || !rng || !payoff) {
        throw std::runtime_error("Missing components for MCBuilder");
    }
    return std::make_unique<MCEngine>(sde, rng, *payoff);
}

std::tuple<FDScheme, long, long> MCBuilder::getParams() const {
    return std::make_tuple(scheme, NSIM, NT);
}