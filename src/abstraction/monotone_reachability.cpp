#include "reachability_backends.hpp"
#include <utility>

namespace symctrl {
namespace {
class MonotoneReachability final : public IReachability {
    const std::shared_ptr<const IDynamics> dynamics_;

public:
    explicit MonotoneReachability(std::shared_ptr<const IDynamics> dynamics)
        : dynamics_(std::move(dynamics)) {}

    std::size_t stateDimension() const noexcept override { return dynamics_->stateDimension(); }
    std::size_t inputDimension() const noexcept override { return dynamics_->inputDimension(); }
    std::size_t disturbanceDimension() const noexcept override { return dynamics_->disturbanceDimension(); }
    NumericGuarantee numericGuarantee() const noexcept override {
        return NumericGuarantee::UncertifiedFloat;
    }

    Box enclose(const Box& cell, const Vector& input, const Box& disturbances) const override {
        validateBox(cell);
        validateVector(cell.lo, stateDimension());
        validateVector(input, inputDimension());
        validateBox(disturbances, true);
        validateVector(disturbances.lo, disturbanceDimension());

        // The supplied model justifies monotonicity on this whole request, not just its centre.
        if (!dynamics_->cooperativeOn(cell, input, disturbances))
            throw UnsupportedCapability("dynamics are not cooperative on the requested boxes/input");

        auto lower = dynamics_->step(cell.lo, input, disturbances.lo);
        validateVector(lower, stateDimension());
        auto upper = dynamics_->step(cell.hi, input, disturbances.hi);
        validateVector(upper, stateDimension());
        Box result{std::move(lower), std::move(upper)};
        validateBox(result); // Reject inconsistent endpoints rather than silently reordering them.
        return result;
    }
};
}

namespace detail {
std::shared_ptr<const IReachability> makeMonotoneReachability(
    std::shared_ptr<const IDynamics> dynamics) {
    return std::make_shared<MonotoneReachability>(std::move(dynamics));
}
}
}
