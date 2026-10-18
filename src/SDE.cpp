#include <SDE.hpp>
#include <cmath>

// GBMModel Implementation
GBMModel::GBMModel(const OptionParams::OptionData& option) : data(std::make_shared<OptionParams::OptionData>(option)) {}

double GBMModel::drift(double t, double S) const {
    return (data->r - data->D) * S;
}

double GBMModel::diffusion(double t, double S) const {
    return data->sig * S;
}

double GBMModel::diffusionDerivative(double t, double S) const {
    return data->sig; // d/dS (σS) = σ
}

double GBMModel::initialCondition() const {
    return data->S0;
}

// CEVModel Implementation
CEVModel::CEVModel(const OptionParams::OptionData& option, double beta_)
    : data(std::make_shared<OptionParams::OptionData>(option)), beta(beta_) {
}

double CEVModel::drift(double t, double S) const {
    return (data->r - data->D) * S;
}

double CEVModel::diffusion(double t, double S) const {
    return data->sig * std::pow(std::max(S, 0.0), beta);
}

double CEVModel::diffusionDerivative(double t, double S) const {
    return data->sig * beta * std::pow(std::max(S, 0.0001), beta - 1.0); // d/dS (σS^β) = σβS^(β-1)
}

double CEVModel::initialCondition() const {
    return data->S0;
}

// CIRModel Implementation
CIRModel::CIRModel(const OptionParams::OptionData& option, double kappa_, double theta_)
    : data(std::make_shared<OptionParams::OptionData>(option)), kappa(kappa_), theta(theta_) {
}

double CIRModel::drift(double t, double S) const {
    return kappa * (theta - S);
}

double CIRModel::diffusion(double t, double S) const {
    return data->sig * std::sqrt(std::max(S, 0.0));
}

double CIRModel::diffusionDerivative(double t, double S) const {
	// formula for the derivative of the diffusion term
	return data->sig / (2.0 * std::sqrt(std::max(S, 0.0001)));  // avoid division by zero
}

double CIRModel::initialCondition() const {
    return data->S0;
}