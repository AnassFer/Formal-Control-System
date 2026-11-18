#include "reachability_backends.hpp"
#include <utility>

namespace symctrl {
namespace {
class MonotoneReachability final : public IReachability {
    const std::shared_ptr<const IDynamics> dynamics_;

public:
    // Input: dynamics: nonnull shared immutable model, already checked by the public factory.
    // Output: Constructs a backend retaining shared ownership of dynamics; no return value.
    explicit MonotoneReachability(std::shared_ptr<const IDynamics> dynamics)
        : dynamics_(std::move(dynamics)) {}

    // Input: None; reads the retained dynamics model.
    // Output: Number n of state coordinates in the retained dynamics.
    std::size_t stateDimension() const noexcept override { return dynamics_->stateDimension(); }
    // Input: None; reads the retained dynamics model.
    // Output: Number m of input coordinates in the retained dynamics.
    std::size_t inputDimension() const noexcept override { return dynamics_->inputDimension(); }
    // Input: None; reads the retained dynamics model.
    // Output: Number q of disturbance coordinates in the retained dynamics; may be zero.
    std::size_t disturbanceDimension() const noexcept override { return dynamics_->disturbanceDimension(); }
    // Input: None; reads this enclosure backend.
    // Output: NumericGuarantee::UncertifiedFloat; rounding errors are not certified.
    NumericGuarantee numericGuarantee() const noexcept override {
        return NumericGuarantee::UncertifiedFloat;
    }

    // Input: cell: closed state box; input: fixed input; disturbances: closed disturbance box, empty when q =
    //        0.
    // Output: Box from lower/upper corner evaluations, conditional on cooperative dynamics; arithmetic is
    //         uncertified.
    // Throws: std::invalid_argument for invalid requests/results; UnsupportedCapability without monotonicity;
    //         model errors propagate.
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
// Input: dynamics: nonnull shared model validated by makeReachability.
// Output: Shared immutable cooperative-monotone backend retaining the model.
std::shared_ptr<const IReachability> makeMonotoneReachability(
    std::shared_ptr<const IDynamics> dynamics) {
    return std::make_shared<MonotoneReachability>(std::move(dynamics));
}
}
}
