// This file is part of the OptionParams library, which provides structures and functions for option pricing.
// It encapsulates option data and provides methods for calculating payoffs based on the option type (call or put).
// Author: Sahil Jain

// ---------------- include/OptionData.hpp ----------------
#pragma once
#include <cmath>
#include <algorithm> // for std::max

namespace OptionParams {

    struct OptionData {
        double S0;  // Initial price
        double K;   // Strike
        double T;   // Expiry
        double r;   // Risk-free rate
        double sig; // Volatility
        double D;   // Dividend
        int type;   // 1 = Call, -1 = Put

		// Constructor for OptionData
		// USing initializer list to set the parameters 
        OptionData(double initial, double strike, double expiration, double interestRate, double volatility, double dividend, int optionType)
            : S0(initial), K(strike), T(expiration), r(interestRate), sig(volatility), D(dividend), type(optionType) {
        }

		// This function calculates the payoff of the option at maturity based on the current stock price S.
		// If the option is a call (type == 1), it returns the maximum of (S - K, 0).
		// If the option is a put (type == -1), it returns the maximum of (K - S, 0).
        virtual double payoff(double S) const {
            if (type == 1) return std::max(S - K, 0.0);
            else return std::max(K - S, 0.0);
        }

        // Virtual clone method to enable thread-safe deep copying in parallel simulations
        virtual std::unique_ptr<OptionData> clone() const {
            return std::make_unique<OptionData>(*this);
        }

		// Virtual destructor to ensure proper cleanup of derived classes
        virtual ~OptionData() = default;
    };

    struct BarrierOptionData : public OptionData {
        double barrier;
        bool isUpAndOut;
        bool knockedOut;

        BarrierOptionData(double initial, double strike, double expiration, double interestRate, double volatility,
            double dividend, int optionType, double barrier_, bool isUpAndOut_)
            : OptionData(initial, strike, expiration, interestRate, volatility, dividend, optionType),
            barrier(barrier_), isUpAndOut(isUpAndOut_), knockedOut(false) {
        }

        double payoff(double S) const override {
            if (knockedOut) return 0.0;
            return OptionData::payoff(S);
        }

        std::unique_ptr<OptionData> clone() const override {
            return std::make_unique<BarrierOptionData>(*this);
        }
    };

    struct AsianOptionData : public OptionData {
        double priceSum;
        long steps;

        AsianOptionData(double initial, double strike, double expiration, double interestRate, double volatility,
            double dividend, int optionType)
            : OptionData(initial, strike, expiration, interestRate, volatility, dividend, optionType),
            priceSum(0.0), steps(0) {
        }

        double payoff(double S) const override {
            double avgPrice = (steps > 0) ? (priceSum / steps) : S;
            if (type == 1) return std::max(avgPrice - K, 0.0);
            else return std::max(K - avgPrice, 0.0);
        }

        std::unique_ptr<OptionData> clone() const override {
            return std::make_unique<AsianOptionData>(*this);
        }
    };

} // namespace OptionParams