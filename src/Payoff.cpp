// This operator allows the Payoff class to be used as a function, making it easier to compute payoffs directly.

#include <Payoff.hpp>

Payoff::Payoff(OptionParams::OptionData& option)
    : ownedData(option.clone()), dataPtr(ownedData.get()) {}

Payoff::Payoff(std::shared_ptr<OptionParams::OptionData> option)
    : ownedData(option ? option->clone() : nullptr), dataPtr(ownedData.get()) {}

Payoff::Payoff(const Payoff& other)
    : ownedData(other.dataPtr ? other.dataPtr->clone() : nullptr), dataPtr(ownedData.get()) {}

Payoff& Payoff::operator=(const Payoff& other) {
    if (this != &other) {
        ownedData = other.dataPtr ? other.dataPtr->clone() : nullptr;
        dataPtr = ownedData.get();
    }
    return *this;
}

double Payoff::operator()(double S) const {
    return dataPtr ? dataPtr->payoff(S) : 0.0;
}

double Payoff::getExpiry() const {
    return dataPtr ? dataPtr->T : 0.0;
}

double Payoff::getRate() const {
    return dataPtr ? dataPtr->r : 0.0;
}

bool Payoff::isBarrierOption() const {
    return dynamic_cast<OptionParams::BarrierOptionData*>(dataPtr) != nullptr;
}

bool Payoff::isAsianOption() const {
    return dynamic_cast<OptionParams::AsianOptionData*>(dataPtr) != nullptr;
}

void Payoff::checkBarrier(double S) {
    auto* barrierOpt = dynamic_cast<OptionParams::BarrierOptionData*>(dataPtr);
    if (barrierOpt) {
        if (barrierOpt->isUpAndOut && S >= barrierOpt->barrier) {
            barrierOpt->knockedOut = true;
        }
        else if (!barrierOpt->isUpAndOut && S <= barrierOpt->barrier) {
            barrierOpt->knockedOut = true;
        }
    }
}

void Payoff::accumulatePrice(double S) {
    auto* asianOpt = dynamic_cast<OptionParams::AsianOptionData*>(dataPtr);
    if (asianOpt) {
        asianOpt->priceSum += S;
        asianOpt->steps++;
    }
}

bool Payoff::isKnockedOut() const {
    auto* barrierOpt = dynamic_cast<OptionParams::BarrierOptionData*>(dataPtr);
    return barrierOpt && barrierOpt->knockedOut;
}

void Payoff::resetBarrier() {
    auto* barrierOpt = dynamic_cast<OptionParams::BarrierOptionData*>(dataPtr);
    if (barrierOpt) {
        barrierOpt->knockedOut = false;
    }
}

void Payoff::resetAsian() {
    auto* asianOpt = dynamic_cast<OptionParams::AsianOptionData*>(dataPtr);
    if (asianOpt) {
        asianOpt->priceSum = 0.0;
        asianOpt->steps = 0;
    }
}