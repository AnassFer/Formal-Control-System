#pragma once
#include "symctrl/contracts/types.hpp"
namespace symctrl {
enum class ReachabilityMethod { CooperativeMonotone, BoundedDerivative };
struct ReachabilityOptions {
    bool require_validated = false; // Reject uncertified backends when true; no tolerance fudge.
};
class IReachability {
public:
    virtual ~IReachability() = default;
    virtual std::size_t stateDimension() const noexcept = 0;
    virtual std::size_t inputDimension() const noexcept = 0;
    virtual std::size_t disturbanceDimension() const noexcept = 0;
    virtual NumericGuarantee numericGuarantee() const noexcept = 0;
    virtual Box enclose(const Box& cell, const Vector& input, const Box& disturbance_box) const = 0;
};
}
