// Description: Header file for the Stochastic Differential Equation (SDE) class
// This file defines the SDE class, which models the dynamics of an option's underlying asset using a stochastic differential equation.
// It includes methods for calculating the drift and diffusion terms, as well as the initial condition for the SDE.

#pragma once
#include "OptionData.hpp"
#include <memory>

class SDEBase {
public:
    virtual ~SDEBase() = default;
    virtual double drift(double t, double S) const = 0;
    virtual double diffusion(double t, double S) const = 0;
    virtual double initialCondition() const = 0;

    virtual double diffusionDerivative(double t, double S) const = 0; // For Milstein
};

// GBM Model (unchanged from Phase 1)
class GBMModel : public SDEBase {
private:
    std::shared_ptr<OptionParams::OptionData> data;
public:
    explicit GBMModel(const OptionParams::OptionData& option);
    double drift(double t, double S) const override;
    double diffusion(double t, double S) const override;
    double initialCondition() const override;
    double diffusionDerivative(double t, double S) const override;
};

// CEV Model
class CEVModel : public SDEBase {
private:
    std::shared_ptr<OptionParams::OptionData> data;
    double beta; // Elasticity parameter
public:
    CEVModel(const OptionParams::OptionData& option, double beta_);
    double drift(double t, double S) const override;
    double diffusion(double t, double S) const override;
    double initialCondition() const override;
    double diffusionDerivative(double t, double S) const override;
};

// CIR Model
class CIRModel : public SDEBase {
private:
    std::shared_ptr<OptionParams::OptionData> data;
    double kappa; // Mean reversion speed
    double theta; // Long-term mean
public:
    CIRModel(const OptionParams::OptionData& option, double kappa_, double theta_);
    double drift(double t, double S) const override;
    double diffusion(double t, double S) const override;
    double initialCondition() const override;
    double diffusionDerivative(double t, double S) const override;
};