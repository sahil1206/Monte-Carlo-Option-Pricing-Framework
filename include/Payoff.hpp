// This file defines the Payoff class, which calculates the payoff of an option based on its parameters.

#pragma once
#include <OptionData.hpp>

class Payoff {
private:
    std::shared_ptr<OptionParams::OptionData> ownedData;
    OptionParams::OptionData* dataPtr;

public:
    explicit Payoff(OptionParams::OptionData& option);
    explicit Payoff(std::shared_ptr<OptionParams::OptionData> option);
    Payoff(const Payoff& other);
    Payoff& operator=(const Payoff& other);
    Payoff(Payoff&&) noexcept = default;
    Payoff& operator=(Payoff&&) noexcept = default;
    ~Payoff() = default;

    double operator()(double S) const;
    double getExpiry() const;
    double getRate() const;

    bool isBarrierOption() const;
    bool isAsianOption() const;
    void checkBarrier(double S);
    void accumulatePrice(double S);
    bool isKnockedOut() const;
    void resetBarrier(); // Reset knockedOut for new path
    void resetAsian();   // Reset priceSum and steps for new path
};